#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <mach/mach.h>
#include <unistd.h>
enum { CAPABILITY_SCALARS = 16 };
_Static_assert(CAPABILITY_SCALARS <= 16, "IOKit scalar response too large");

static io_connect_t connection(void) {
    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("TXGamingFanBridge"));
    if (!service) return IO_OBJECT_NULL;
    io_connect_t client = IO_OBJECT_NULL;
    IOReturn result = IOServiceOpen(service, mach_task_self(), 0, &client);
    IOObjectRelease(service);
    return result == kIOReturnSuccess ? client : IO_OBJECT_NULL;
}

static NSDictionary *reader(void) {
    NSString *binary = NSProcessInfo.processInfo.arguments[0].stringByDeletingLastPathComponent;
    binary = [binary stringByAppendingPathComponent:@"statusreader"];
    NSTask *task = [NSTask new];
    task.executableURL = [NSURL fileURLWithPath:binary];
    task.arguments = @[@"read"];
    NSPipe *pipe = [NSPipe pipe];
    task.standardOutput = pipe;
    task.standardError = [NSFileHandle fileHandleWithNullDevice];
    NSError *error = nil;
    if (![task launchAndReturnError:&error]) return @{};
    NSData *data = [pipe.fileHandleForReading readDataToEndOfFile];
    [task waitUntilExit];
    if (task.terminationStatus) return @{};
    id parsed = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    return [parsed isKindOfClass:NSDictionary.class] ? parsed : @{};
}

static void json(NSDictionary *value) {
    NSData *data = [NSJSONSerialization dataWithJSONObject:value options:NSJSONWritingSortedKeys error:nil];
    fwrite(data.bytes, 1, data.length, stdout);
    puts("");
}

int main(int argc, char **argv) {
    @autoreleasepool {
        if (argc < 2) { fprintf(stderr, "usage: txfanctl status | capabilities | profile 0|1|2\n"); return 2; }
        NSString *command = @(argv[1]);
        if ([command isEqualToString:@"capabilities"]) {
            uint64_t output[CAPABILITY_SCALARS] = {0}; uint32_t count = CAPABILITY_SCALARS;
            uint64_t fanMode[2] = {UINT64_MAX,UINT64_MAX}; uint32_t modeCount = 2;
            io_connect_t client = connection();
            IOReturn result = client ? IOConnectCallScalarMethod(client, 3, NULL, 0, output, &count) : kIOReturnNotFound;
            IOReturn modeResult = client ? IOConnectCallScalarMethod(client, 4, NULL, 0, fanMode, &modeCount) : kIOReturnNotFound;
            if (client) IOServiceClose(client);
            BOOL available = result == kIOReturnSuccess && count == CAPABILITY_SCALARS && output[0] == 3;
            BOOL modeAvailable = modeResult == kIOReturnSuccess && modeCount == 2;
            NSMutableArray *devices = [NSMutableArray array];
            if (available) {
                for (unsigned i = 0; i < 3; ++i) {
                    unsigned b = 1+i*5;
                    [devices addObject:@{@"deviceID":[NSString stringWithFormat:@"0x%08llX",output[b]],
                                         @"statusRaw":@(output[b+1]), @"statusResult":@(output[b+2]),
                                         @"readCapability":output[b+3]==UINT64_MAX ? NSNull.null : @(output[b+3]),
                                         @"writeCapability":output[b+4]==UINT64_MAX ? NSNull.null : @(output[b+4])}];
                }
            }
            json(@{@"capabilitiesAvailable":@(available), @"result":@(result), @"devices":devices,
                   @"firmwareFanMode":modeAvailable && fanMode[1]==0 ? @(fanMode[0]) : NSNull.null,
                   @"firmwareFanModeResult":modeAvailable ? @(fanMode[1]) : @(modeResult)});
            return 0;
        }
        if ([command isEqualToString:@"status"]) {
            NSDictionary *smc = reader();
            double temperature = 0;
            for (NSDictionary *sensor in smc[@"sensors"]) {
                NSString *key = sensor[@"key"];
                double value = [sensor[@"value"] doubleValue];
                if ([key hasPrefix:@"TC"] && value >= 5 && value <= 125) temperature = MAX(temperature, value);
            }
            uint64_t output[6] = {0}; uint32_t count = 6;
            io_connect_t client = connection();
            IOReturn result = client ? IOConnectCallScalarMethod(client, 0, NULL, 0, output, &count) : kIOReturnNotFound;
            if (client) IOServiceClose(client);
            BOOL connected = result == kIOReturnSuccess && count == 6 && output[0] == 1;
            uint64_t flags = connected ? output[1] : 0;
            json(@{@"model":@"ASUS TX Gaming FX608LM", @"bridgeConnected":@(connected),
                   @"cpuTemperature":temperature ? @(temperature) : NSNull.null,
                   @"cpuFanRPM":(flags & 1) ? @(output[2]) : NSNull.null,
                   @"gpuFanRPM":(flags & 2) ? @(output[3]) : NSNull.null,
                   @"profileSupported":((flags & 4) && (flags & 3)) ? @YES : @NO, @"profileChangesEnabled":(flags & 8) ? @YES : @NO,
                   @"profile":(flags & 4) ? @(output[4]) : NSNull.null,
                   @"logicalCPUs":@(NSProcessInfo.processInfo.activeProcessorCount),
                   @"memoryGiB":@(NSProcessInfo.processInfo.physicalMemory / (1024.0*1024*1024)),
                   @"bridgeResult":@(result)});
            return 0;
        }
        if ([command isEqualToString:@"profile"]) {
            if (argc != 3 || strlen(argv[2]) != 1 || argv[2][0] < '0' || argv[2][0] > '2') {
                fprintf(stderr, "Only firmware profiles 0, 1 and 2 are allowed.\n"); return 2;
            }
            if (geteuid() != 0) { fprintf(stderr, "Administrator authorization is required.\n"); return 3; }
            io_connect_t client = connection();
            if (!client) { fprintf(stderr, "The ASUS fan bridge is not loaded.\n"); return 4; }
            uint64_t armed = 0; uint32_t armCount = 1;
            IOReturn armResult = IOConnectCallScalarMethod(client, 2, NULL, 0, &armed, &armCount);
            if (armResult != kIOReturnSuccess || armCount != 1 || armed != 1) {
                IOServiceClose(client);
                fprintf(stderr, "Firmware support is not verified (0x%08x).\n", armResult);
                return 5;
            }
            uint64_t input = argv[2][0] - '0', output = 0; uint32_t count = 1;
            IOReturn result = IOConnectCallScalarMethod(client, 1, &input, 1, &output, &count);
            IOServiceClose(client);
            if (result != kIOReturnSuccess || count != 1 || output != input) {
                fprintf(stderr, "Firmware profile not confirmed (0x%08x).\n", result); return 5;
            }
            json(@{@"ok":@YES, @"profile":@(output)});
            return 0;
        }
        fprintf(stderr, "Unsupported command. Raw EC writes and PWM commands are unavailable.\n");
        return 2;
    }
}
