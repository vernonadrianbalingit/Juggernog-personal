/*
 * frame.h — Pourtocol binary frame format
 *
 * Wire layout (big-endian for all multi-byte fields):
 *
 * offset 0: MAGIC (2 bytes) always 0x50 0x43
 * offset 2: VERSION (1 byte)
 * offset 3: TYPE (1 byte)
 * offset 4: SEQUENCE (4 bytes)
 * offset 8: LENGTH (2 bytes) payload length
 * offset 10: PAYLOAD (LENGTH bytes)
 * offset 10+LENGTH: CRC32 (4 bytes)
 *
 * CRC-32 is computed over bytes [0 .. 10+LENGTH-1], usingg the
 * IEEE 802.3 polynomial (the one Ethernet uses), initial value 0xFFFFFFFF,
 * final XOR 0xFFFFFFFF, reflected input/output. Matches "crc32" in zlib.
 */
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
namespace juggernog
{
  constexpr uint16_t MAGIC = 0x5043; // 'P','C'
  constexpr uint8_t VERSION = 0x01;
  constexpr size_t HEADER_SIZE = 10; // magic(2)+ver(1)+type(1)+seq(4)+len(2)
  constexpr size_t CRC_SIZE = 4;
  constexpr size_t FRAMING_OVERHEAD = HEADER_SIZE + CRC_SIZE;
  constexpr size_t MAX_PAYLOAD = 1024; // intentionally small for now
  // Message types
enum class MsgType : uint8_t {
PING = 0x01,
PONG = 0x02,
DISPENSE_REQ = 0x10,
DISPENSE_ACK = 0x11,
STATUS_REQ = 0x20,
STATUS_RES = 0x21,
};
// Outcomes of parsing a frame
enum class ParseResult
{
  OK,
  TOO_SHORT,       // didn't get enough bytes for even a header
  BAD_MAGIC,       // first two bytes don't match
  BAD_VERSION,     // we don't speak this version
  LENGTH_MISMATCH, // declared length doesn't match buffer size
  BAD_CRC,         // CRC didn't validate
};
// A parsed frame in memory.
struct Frame
{
  uint8_t version;
  MsgType type;
  uint32_t sequence;
  std::vector<uint8_t> payload;
};
// Serialize a Frame into a byte buffer, ready to send over the wire.
// Returns the buffer (header + payload + crc).
std::vector<uint8_t> serialize(const Frame &frame);
// Parse a buffer into a Frame. Returns the result code; on OK, fills `out`.
ParseResult parse(const uint8_t *buffer, size_t length, Frame &out);
// Convenience: convert a ParseResult to a human-readable string.
const char *parse_result_to_str(ParseResult r);
} // namespace juggernog