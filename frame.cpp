#include "frame.h"

#include <arpa/inet.h>  // htonl, htons, ntohl, ntohs

#include <cstring>  // memcpy

#include "crc32.h"
namespace juggernog {
// Same htons/htonl idea as the sockets code. We go through a temp with
// memcpy instead of casting the buffer pointer to uint16_t* because the
// buffer isn't guaranteed to be aligned and that cast is technically UB
namespace {
void write_u16(uint8_t* buf, uint16_t v) {
  uint16_t be = htons(v);
  std::memcpy(buf, &be, 2);
}
void write_u32(uint8_t* buf, uint32_t v) {
  uint32_t be = htonl(v);
  std::memcpy(buf, &be, 4);
}
uint16_t read_u16(const uint8_t* buf) {
  uint16_t be;
  std::memcpy(&be, buf, 2);
  return ntohs(be);
}
uint32_t read_u32(const uint8_t* buf) {
  uint32_t be;
  std::memcpy(&be, buf, 4);
  return ntohl(be);
}
}  // anonymous namespace
std::vector<uint8_t> serialize(const Frame& frame) {
  const size_t total = HEADER_SIZE + frame.payload.size() + CRC_SIZE;
  std::vector<uint8_t> buf(total);
  // Header
  write_u16(&buf[0], MAGIC);
  buf[2] = frame.version;
  buf[3] = static_cast<uint8_t>(frame.type);
  write_u32(&buf[4], frame.sequence);
  write_u16(&buf[8], static_cast<uint16_t>(frame.payload.size()));
  // Payload
  if (!frame.payload.empty()) {
    std::memcpy(&buf[HEADER_SIZE], frame.payload.data(), frame.payload.size());
  }
  // CRC goes last and covers header + payload (everything before it).
  // Receiver recomputes over the same range and compares
  uint32_t crc = crc32(buf.data(), HEADER_SIZE + frame.payload.size());
  write_u32(&buf[HEADER_SIZE + frame.payload.size()], crc);
  return buf;
}
ParseResult parse(const uint8_t* buffer, size_t length, Frame& out) {
  // STEP 1: Do we even have enough bytes for a header and a CRC?
  //  If not there's no point reading any fields, we'd run off the buffer
  if (length < HEADER_SIZE + CRC_SIZE) {
    return ParseResult::TOO_SHORT;
  }
  // STEP 2: Magic bytes. Anything that doesn't start with "PC" is not ours
  //  (random UDP traffic, someone running the phase 1 client, etc)
  uint16_t magic = read_u16(&buffer[0]);
  if (magic != MAGIC) {
    return ParseResult::BAD_MAGIC;
  }
  // STEP 3: Version. We only speak v1 right now
  uint8_t version = buffer[2];
  if (version != VERSION) {
    return ParseResult::BAD_VERSION;
  }
  // STEP 4: LENGTH field has to agree with how many bytes recvfrom actually
  //  handed us. If it doesn't, the packet got truncated or padded somewhere
  uint16_t payload_len = read_u16(&buffer[8]);
  size_t expected = HEADER_SIZE + payload_len + CRC_SIZE;
  if (length != expected) {
    return ParseResult::LENGTH_MISMATCH;
  }
  //  also cap it, a bogus length should not be able to push us past MAX_PAYLOAD
  if (payload_len > MAX_PAYLOAD) {
    return ParseResult::LENGTH_MISMATCH;
  }
  // STEP 5: Recompute the CRC over header + payload and compare against the
  //  last 4 bytes the sender put there. Any flipped bit in between and these
  //  won't match
  uint32_t expected_crc = read_u32(&buffer[HEADER_SIZE + payload_len]);
  uint32_t actual_crc = crc32(buffer, HEADER_SIZE + payload_len);
  if (expected_crc != actual_crc) {
    return ParseResult::BAD_CRC;
  }
  // STEP 6: everything checked out, now it's safe to copy the fields out
  out.version = version;
  out.type = static_cast<MsgType>(buffer[3]);
  out.sequence = read_u32(&buffer[4]);
  out.payload.assign(&buffer[HEADER_SIZE], &buffer[HEADER_SIZE + payload_len]);
  return ParseResult::OK;
}
const char* parse_result_to_str(ParseResult r) {
  switch (r) {
    case ParseResult::OK:
      return "OK";
    case ParseResult::TOO_SHORT:
      return "TOO_SHORT";
    case ParseResult::BAD_MAGIC:
      return "BAD_MAGIC";
    case ParseResult::BAD_VERSION:
      return "BAD_VERSION";
    case ParseResult::LENGTH_MISMATCH:
      return "LENGTH_MISMATCH";
    case ParseResult::BAD_CRC:
      return "BAD_CRC";
  }
  return "UNKNOWN";
}
}  // namespace juggernog