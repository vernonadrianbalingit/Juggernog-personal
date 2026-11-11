#pragma once
#include <cstddef>
#include <cstdint>
namespace juggernog {
// Plain CRC-32, same params as zlib.crc32 so we can cross check in python.
//  polynomial 0xEDB88320 (thats 0x04C11DB7 reflected), init 0xFFFFFFFF,
//  xor the result with 0xFFFFFFFF at the end
uint32_t crc32(const uint8_t* data, size_t length);
}  // namespace juggernog