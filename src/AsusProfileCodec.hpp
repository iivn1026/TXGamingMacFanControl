#ifndef TXGAMING_ASUS_PROFILE_CODEC_HPP
#define TXGAMING_ASUS_PROFILE_CODEC_HPP
#include <stdint.h>
// Endpoint status and actual FANM mode are distinct values.
constexpr bool asusEndpointPresent(uint32_t raw) {
    return (raw & 0x00010000U) != 0 && raw != 0xFFFFFFFFU && raw != 0xFFFFFFFEU;
}
constexpr bool validAsusFirmwareMode(uint32_t mode) {
    return mode <= 4;
}
constexpr bool writableAsusProfileTarget(uint64_t target) {
    return target <= 2;
}
#endif
