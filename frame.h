/*
frame.h - Pourtocol binary frame format

Phase 2: instead of raw bytes, every UDP packet now carries one of these.
All multi-byte fields are big-endian (network order), same reason we
htons() the port in the sockets code.

  offset 0:         MAGIC    (2 bytes)  always 0x50 0x43, "PC"
  offset 2:         VERSION  (1 byte)
  offset 3:         TYPE     (1 byte)   see MsgType below
  offset 4:         SEQUENCE (4 bytes)
  offset 8:         LENGTH   (2 bytes)  how many payload bytes follow
  offset 10:        PAYLOAD  (LENGTH bytes)
  offset 10+LENGTH: CRC32    (4 bytes)

The CRC covers everything from offset 0 up to (not including) the CRC
field itself. It's the standard CRC-32 everyone uses (Ethernet, zlib,
PNG). Easy way to check a frame by hand: python3 -c "import zlib; ..."
gives the same number.
*/
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace juggernog {
constexpr uint16_t MAGIC = 0x5043;  // 'P','C'
constexpr uint8_t VERSION = 0x01;
constexpr size_t HEADER_SIZE = 10;  // magic(2)+ver(1)+type(1)+seq(4)+len(2)
constexpr size_t CRC_SIZE = 4;
constexpr size_t FRAMING_OVERHEAD = HEADER_SIZE + CRC_SIZE;
constexpr size_t MAX_PAYLOAD = 1024;  // keeping this small for now, our
                                      // messages are tiny and it stays well
                                      // under the recv buffer
// Message types
enum class MsgType : uint8_t {
  PING = 0x01,
  PONG = 0x02,
  ACK = 0x03,
  DISPENSE_REQ = 0x10,
  DISPENSE_ACK = 0x11,
  STATUS_REQ = 0x20,
  STATUS_RES = 0x21,
};
// Outcomes of parsing a frame
enum class ParseResult {
  OK,
  TOO_SHORT,        // didn't get enough bytes for even a header
  BAD_MAGIC,        // first two bytes don't match
  BAD_VERSION,      // we don't speak this version
  LENGTH_MISMATCH,  // declared length doesn't match buffer size
  BAD_CRC,          // CRC didn't validate
};
// What a frame looks like once we've pulled it apart. The payload gets
// copied out of the recv buffer so the buffer can be reused.
struct Frame {
  uint8_t version;
  MsgType type;
  uint32_t sequence;
  std::vector<uint8_t> payload;
};
// Frame -> bytes. Builds header + payload + crc in one vector, hand the
// result straight to sendto()
std::vector<uint8_t> serialize(const Frame& frame);
// bytes -> Frame. Takes whatever recvfrom() gave us and runs all the
// checks. Only touches `out` if it returns OK, so on any error `out` is
// whatever it was before
ParseResult parse(const uint8_t* buffer, size_t length, Frame& out);
// so the server can print WHY it dropped a packet instead of just a number
const char* parse_result_to_str(ParseResult r);
}  // namespace juggernog