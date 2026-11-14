#include "crc32.h"
namespace juggernog {
// 256 entry lookup table for CRC-32. Doing it bit by bit is 8 shifts per
// byte, with the table it's one lookup + one xor per byte. The table gets
// built once at program start (static init of TABLE below) and never changes
namespace {
class Crc32Table {
 public:
  Crc32Table() {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int j = 0; j < 8; ++j) {
        c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      }
      table_[i] = c;
    }
  }
  uint32_t operator[](uint8_t i) const { return table_[i]; }

 private:
  uint32_t table_[256];
};
const Crc32Table TABLE;
}  // anonymous namespace
uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < length; ++i) {
    crc = TABLE[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFFu;
}
}  // namespace juggernog