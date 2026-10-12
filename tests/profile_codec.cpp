#include "../src/AsusProfileCodec.hpp"
#include <cassert>
int main() {
    // Actual FX608LM result captured by the 0.3 capability probe.
    assert(asusEndpointPresent(65539));
    assert(!asusEndpointPresent(3));
    assert(!asusEndpointPresent(0xFFFFFFFEU));
    assert(!asusEndpointPresent(0xFFFFFFFFU));
    for (uint32_t mode=0; mode<=4; ++mode) assert(validAsusFirmwareMode(mode));
    assert(!validAsusFirmwareMode(5));
    for (uint64_t mode=0; mode<=2; ++mode) assert(writableAsusProfileTarget(mode));
    assert(!writableAsusProfileTarget(3));
    assert(!writableAsusProfileTarget(4));
    assert(!writableAsusProfileTarget(UINT64_MAX));
}
