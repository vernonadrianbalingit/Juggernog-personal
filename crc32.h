#pragma once
#include <cstdint>
#include <cstddef>
namespace juggernog {
// IEEE 802.3 CRC-32 (the one Ethernet uses, also used by zlib, PNG, gzip).
// Polynomial: 0xEDB88320 (reflected). Initial: 0xFFFFFFFF. Final XOR: 0xFFFFFFFF.
uint32_t crc32(const uint8_t* data, size_t length);
} // namespace juggernog