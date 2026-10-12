// TX Gaming FX608LM WMI bridge. No raw EC/MMIO writes or manual PWM commands.
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLocks.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSNumber.h>
#include <mach/kmod.h>
#include "AsusProfileCodec.hpp"

static const uint32_t DSTS = 0x53545344, DEVS = 0x53564544;
static const uint32_t CPU_FAN = 0x00110013, GPU_FAN = 0x00110014;
static const uint32_t POLICY = 0x00120075, PRESENT = 0x00010000;
static const uint32_t CAPABILITY_SCALARS = 16;
static_assert(CAPABILITY_SCALARS <= kIOExternalMethodScalarOutputCountMax,
              "Capability response exceeds the IOKit scalar transport limit");

class TXGamingFanClient;
class TXGamingFanBridge : public IOService {
    OSDeclareDefaultStructors(TXGamingFanBridge)
    IOACPIPlatformDevice *device = nullptr;
    IOLock *lock = nullptr;
    bool online = false, enableChanges = false;
    IOReturn wmi(uint32_t method, uint32_t id, uint32_t value, uint32_t *result);
    IOReturn profileState(uint32_t *result);
public:
    IOService *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;
    IOReturn newUserClient(task_t task, void *security, UInt32 type, IOUserClient **handler) override;
    IOReturn snapshot(uint64_t *out);
    IOReturn capabilities(uint64_t *out);
    IOReturn firmwareMode(uint64_t *out);
    IOReturn setProfile(uint32_t value, uint64_t *result, bool clientArmed);
};

class TXGamingFanClient : public IOUserClient {
    OSDeclareDefaultStructors(TXGamingFanClient)
    TXGamingFanBridge *bridge = nullptr;
    bool administrator = false;
    bool armed = false;
public:
    bool initWithTask(task_t task, void *security, UInt32 type) override;
    bool start(IOService *provider) override;
    IOReturn clientClose() override;
    void stop(IOService *provider) override;
    IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                           IOExternalMethodDispatch *dispatch = nullptr,
                           OSObject *target = nullptr, void *reference = nullptr) override;
};

OSDefineMetaClassAndStructors(TXGamingFanBridge, IOService)
OSDefineMetaClassAndStructors(TXGamingFanClient, IOUserClient)

IOService *TXGamingFanBridge::probe(IOService *provider, SInt32 *score) {
    if (!IOService::probe(provider, score)) return nullptr;
    auto acpi = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (!acpi || acpi->validateObject("WMNB") != kIOReturnSuccess) return nullptr;
    OSObject *uid = nullptr;
    if (acpi->evaluateObject("_UID", &uid) != kIOReturnSuccess || !uid) return nullptr;
    auto text = OSDynamicCast(OSString, uid);
    bool valid = text && text->isEqualTo("ATK");
    uid->release();
    return valid ? this : nullptr;
}

bool TXGamingFanBridge::start(IOService *provider) {
    if (!IOService::start(provider)) return false;
    device = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (!device) return false;
    device->retain();
    lock = IOLockAlloc();
    if (!lock) return false;
    online = true;
    enableChanges = getProperty("EnableProfileChanges") == kOSBooleanTrue;
    setProperty("BridgeVersion", "0.4.0");
    setProperty("TargetModel", "ASUS TX Gaming FX608LM");
    setProperty("ProfileChangesEnabled", enableChanges);
    registerService();
    return true;
}

void TXGamingFanBridge::stop(IOService *provider) {
    if (lock) { IOLockLock(lock); online = false; IOLockUnlock(lock); }
    IOService::stop(provider);
}
void TXGamingFanBridge::free() {
    if (device) device->release();
    if (lock) IOLockFree(lock);
    IOService::free();
}

IOReturn TXGamingFanBridge::wmi(uint32_t method, uint32_t id, uint32_t value, uint32_t *result) {
    if (!online || !device) return kIOReturnNotReady;
    // FX608LM WMNB creates FIVE DWORD fields from Arg2: an 8-byte buffer is invalid.
    uint32_t payload[5] = {id, value, 0, 0, 0};
    OSObject *args[3] = { OSNumber::withNumber(static_cast<uint64_t>(0), 32), OSNumber::withNumber(static_cast<uint64_t>(method), 32),
                         OSData::withBytes(payload, sizeof(payload)) };
    if (!args[0] || !args[1] || !args[2]) {
        for (unsigned i = 0; i < 3; ++i) if (args[i]) args[i]->release();
        return kIOReturnNoMemory;
    }
    uint32_t valueOut = 0;
    auto status = device->evaluateInteger("WMNB", &valueOut, args, 3);
    for (unsigned i = 0; i < 3; ++i) args[i]->release();
    if (status != kIOReturnSuccess) return status;
    *result = valueOut;
    if (valueOut == 0xFFFFFFFE || valueOut == 0xFFFFFFFF) return kIOReturnUnsupported;
    return kIOReturnSuccess;
}

IOReturn TXGamingFanBridge::capabilities(uint64_t *out) {
    IOLockLock(lock);
    if (!online) { IOLockUnlock(lock); return kIOReturnNotReady; }
    out[0] = 3;
    const uint32_t ids[3] = {POLICY, 0x00110019, 0x001200A4};
    for (unsigned i = 0; i < 3; ++i) {
        unsigned base = 1 + i * 5;
        uint32_t raw = UINT32_MAX;
        out[base] = ids[i];
        out[base + 2] = static_cast<uint32_t>(wmi(DSTS, ids[i], 0, &raw));
        out[base + 1] = raw;
        // WCHK queries a capability table. Arg0=1 asks about write support;
        // it does NOT invoke the DEVS setter or change a fan profile.
        for (unsigned write = 0; write < 2; ++write) {
            OSObject *args[2] = {OSNumber::withNumber(static_cast<uint64_t>(write),32),
                                 OSNumber::withNumber(static_cast<uint64_t>(ids[i]),32)};
            uint32_t cap = UINT32_MAX;
            IOReturn status = kIOReturnNoMemory;
            if (args[0] && args[1]) status = device->evaluateInteger("WCHK", &cap, args, 2);
            for (unsigned a = 0; a < 2; ++a) if (args[a]) args[a]->release();
            out[base + 3 + write] = status == kIOReturnSuccess ? cap : UINT64_MAX;
        }
    }
    IOLockUnlock(lock);
    return kIOReturnSuccess;
}

IOReturn TXGamingFanBridge::firmwareMode(uint64_t *out) {
    IOLockLock(lock);
    if (!online) { IOLockUnlock(lock); return kIOReturnNotReady; }
    uint32_t mode = UINT32_MAX;
    auto status = device->evaluateInteger("TGFM", &mode);
    out[0] = mode; out[1] = static_cast<uint32_t>(status);
    IOLockUnlock(lock);
    return kIOReturnSuccess;
}

// Called while the bridge lock is held. A write-only standard ASUS endpoint
// is accepted only if firmware explicitly advertises it and FANM can be read.
IOReturn TXGamingFanBridge::profileState(uint32_t *result) {
    // DSTS advertises capabilities here, not a dependable current mode.
    // Readback uses FANM through an ACPI method wrapper.
    if (device->validateObject("TGFM") != kIOReturnSuccess) return kIOReturnNotReady;
    OSObject *args[2] = {OSNumber::withNumber(static_cast<uint64_t>(1),32),
                         OSNumber::withNumber(static_cast<uint64_t>(POLICY),32)};
    uint32_t capability = 0;
    IOReturn status = kIOReturnNoMemory;
    if (args[0] && args[1]) status = device->evaluateInteger("WCHK", &capability, args, 2);
    for (unsigned i=0; i<2; ++i) if (args[i]) args[i]->release();
    if (status != kIOReturnSuccess || capability != 1) return kIOReturnUnsupported;
    uint32_t mode = UINT32_MAX;
    status = device->evaluateInteger("TGFM", &mode);
    if (status != kIOReturnSuccess || !validAsusFirmwareMode(mode)) return kIOReturnUnsupported;
    *result = mode;
    return kIOReturnSuccess;
}

IOReturn TXGamingFanBridge::snapshot(uint64_t *out) {
    IOLockLock(lock);
    if (!online) { IOLockUnlock(lock); return kIOReturnNotReady; }
    out[0] = 1; out[1] = 0; out[2] = out[3] = 0; out[4] = UINT32_MAX; out[5] = 0;
    uint32_t raw = 0;
    if (wmi(DSTS, CPU_FAN, 0, &raw) == kIOReturnSuccess && (raw & PRESENT)) {
        uint32_t rpm = (raw & 0xFFFF) * 100;
        if (rpm <= 20000) { out[1] |= 1; out[2] = rpm; }
    }
    if (wmi(DSTS, GPU_FAN, 0, &raw) == kIOReturnSuccess && (raw & PRESENT)) {
        uint32_t rpm = (raw & 0x7FFF) * 100;
        if (rpm <= 20000) { out[1] |= 2; out[3] = rpm; }
    }
    if (profileState(&raw) == kIOReturnSuccess) {
        out[1] |= 4; out[4] = raw; out[5] = POLICY;
    }
    if (enableChanges) out[1] |= 8;
    IOLockUnlock(lock);
    return kIOReturnSuccess;
}

IOReturn TXGamingFanBridge::setProfile(uint32_t value, uint64_t *result, bool clientArmed) {
    if (!writableAsusProfileTarget(value)) return kIOReturnBadArgument;
    IOLockLock(lock);
    IOReturn status = kIOReturnNotPermitted;
    uint32_t current = 0, written = 0, confirmed = 0;
    if ((enableChanges || clientArmed) && online) {
        status = profileState(&current);
        bool writeCompleted = false, writeAttempted = false;
        if (status == kIOReturnSuccess) {
            writeAttempted = true;
            status = wmi(DEVS, POLICY, value, &written);
            writeCompleted = status == kIOReturnSuccess;
        }
        if (writeCompleted) {
            for (unsigned attempt=0; attempt<5; ++attempt) {
                status = profileState(&confirmed);
                if (status == kIOReturnSuccess && confirmed == value) break;
                IOSleep(20);
            }
        }
        if (writeAttempted && (status != kIOReturnSuccess || confirmed != value)) {
            // Restore the known previous firmware profile on a failed confirmation.
            wmi(DEVS, POLICY, current, &written);
            status = kIOReturnIOError;
        }
        if (status == kIOReturnSuccess) *result = confirmed;
    }
    IOLockUnlock(lock);
    return status;
}

bool TXGamingFanClient::initWithTask(task_t task, void *security, UInt32 type) {
    if (type != 0 || !IOUserClient::initWithTask(task, security, type)) return false;
    administrator = clientHasPrivilege(task, kIOClientPrivilegeAdministrator) == kIOReturnSuccess;
    setProperty("IOUserClientDefaultLocking", true);
    return true;
}
bool TXGamingFanClient::start(IOService *provider) {
    bridge = OSDynamicCast(TXGamingFanBridge, provider);
    if (!bridge || !IOUserClient::start(provider)) return false;
    bridge->retain();
    return true;
}
void TXGamingFanClient::stop(IOService *provider) {
    if (bridge) { bridge->release(); bridge = nullptr; }
    IOUserClient::stop(provider);
}
IOReturn TXGamingFanClient::clientClose() { terminate(); return kIOReturnSuccess; }
IOReturn TXGamingFanClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                         IOExternalMethodDispatch *, OSObject *, void *) {
    if (!bridge || !args) return kIOReturnNotReady;
    if (!args->scalarOutput || args->structureInputSize || args->structureOutputSize) return kIOReturnBadArgument;
    if (selector == 0 && args->scalarInputCount == 0 && args->scalarOutputCount == 6) return bridge->snapshot(args->scalarOutput);
    if (selector == 3 && args->scalarInputCount == 0 && args->scalarOutputCount == CAPABILITY_SCALARS) return bridge->capabilities(args->scalarOutput);
    if (selector == 4 && args->scalarInputCount == 0 && args->scalarOutputCount == 2) return bridge->firmwareMode(args->scalarOutput);
    if (selector == 1 && args->scalarInputCount == 1 && args->scalarOutputCount == 1) {
        if (!args->scalarInput) return kIOReturnBadArgument;
        if (!administrator) return kIOReturnNotPrivileged;
        if (!armed) return kIOReturnNotPermitted;
        if (!writableAsusProfileTarget(args->scalarInput[0])) return kIOReturnBadArgument;
        return bridge->setProfile(static_cast<uint32_t>(args->scalarInput[0]), args->scalarOutput, armed);
    }
    // Arming is per administrator connection and makes no thermal change.
    if (selector == 2 && args->scalarInputCount == 0 && args->scalarOutputCount == 1) {
        if (!administrator) return kIOReturnNotPrivileged;
        uint64_t state[6] = {0};
        IOReturn status = bridge->snapshot(state);
        if (status != kIOReturnSuccess) return status;
        if (!(state[1] & 4) || !(state[1] & 3)) return kIOReturnUnsupported;
        armed = true;
        args->scalarOutput[0] = 1;
        return kIOReturnSuccess;
    }
    return kIOReturnBadArgument;
}
IOReturn TXGamingFanBridge::newUserClient(task_t task, void *security, UInt32 type, IOUserClient **handler) {
    auto client = new TXGamingFanClient;
    if (!client) return kIOReturnNoMemory;
    if (!client->initWithTask(task, security, type) || !client->attach(this) || !client->start(this)) {
        if (client->getProvider()) client->detach(this);
        client->release();
        return kIOReturnError;
    }
    *handler = client;
    return kIOReturnSuccess;
}

extern "C" kern_return_t txfan_start(kmod_info_t *, void *) { return KERN_SUCCESS; }
extern "C" kern_return_t txfan_stop(kmod_info_t *, void *) { return KERN_SUCCESS; }
extern "C" {
KMOD_EXPLICIT_DECL(local.TXGamingFanBridge, "0.4.0", txfan_start, txfan_stop)
__attribute__((visibility("default"))) kmod_start_func_t *_realmain = txfan_start;
__attribute__((visibility("default"))) kmod_stop_func_t *_antimain = txfan_stop;
__attribute__((visibility("default"))) int _kext_apple_cc = __APPLE_CC__;
}
