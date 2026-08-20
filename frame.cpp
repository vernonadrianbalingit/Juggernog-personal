#include "frame.h"
#include "crc32.h"
#include <arpa/inet.h> // htonl, htons, ntohl, ntohs
#include <cstring>     // memcpy
namespace juggernog
{
  // Helpers to read/write big-endian fields into byte buffers.
  namespace
  {
    void write_u16(uint8_t *buf, uint16_t v)
    {
      uint16_t be = htons(v);
      std::memcpy(buf, &be, 2);
    }
    void write_u32(uint8_t *buf, uint32_t v)
    {
      uint32_t be = htonl(v);
      std::memcpy(buf, &be, 4);
    }
    uint16_t read_u16(const uint8_t *buf)
    {
      uint16_t be;
      std::memcpy(&be, buf, 2);
      return ntohs(be);
    }
    uint32_t read_u32(const uint8_t *buf)
    {
      uint32_t be;
      std::memcpy(&be, buf, 4);
      return ntohl(be);
    }
  } // anonymous namespace
  std::vector<uint8_t> serialize(const Frame &frame)
  {
    const size_t total = HEADER_SIZE + frame.payload.size() + CRC_SIZE;
    std::vector<uint8_t> buf(total);
    // Header
    write_u16(&buf[0], MAGIC);
    buf[2] = frame.version;
    buf[3] = static_cast<uint8_t>(frame.type);
    write_u32(&buf[4], frame.sequence);
    write_u16(&buf[8], static_cast<uint16_t>(frame.payload.size()));
    // Payload
    if (!frame.payload.empty())
    {
      std::memcpy(&buf[HEADER_SIZE], frame.payload.data(), frame.payload.size());
    }
    // CRC over everything before the CRC field itself
    uint32_t crc = crc32(buf.data(), HEADER_SIZE + frame.payload.size());
    write_u32(&buf[HEADER_SIZE + frame.payload.size()], crc);
    return buf;
  }
  ParseResult parse(const uint8_t *buffer, size_t length, Frame &out)
  {
    // 1. Length sanity
    if (length < HEADER_SIZE + CRC_SIZE)
    {
      return ParseResult::TOO_SHORT;
    }
    // 2. Magic
    uint16_t magic = read_u16(&buffer[0]);
    if (magic != MAGIC)
    {
      return ParseResult::BAD_MAGIC;
    }
    // 3. Version
    uint8_t version = buffer[2];
    if (version != VERSION)
    {
      return ParseResult::BAD_VERSION;
    }
    // 4. Length field
    uint16_t payload_len = read_u16(&buffer[8]);
    size_t expected = HEADER_SIZE + payload_len + CRC_SIZE;
    if (length != expected)
    {
      return ParseResult::LENGTH_MISMATCH;
    }
    if (payload_len > MAX_PAYLOAD)
    {
      return ParseResult::LENGTH_MISMATCH;
    }
    // 5. CRC validation — recompute and compare
    uint32_t expected_crc = read_u32(&buffer[HEADER_SIZE + payload_len]);
    uint32_t actual_crc = crc32(buffer, HEADER_SIZE + payload_len);
    if (expected_crc != actual_crc)
    {
      return ParseResult::BAD_CRC;
    }
    // 6. All good — fill the output struct
    out.version = version;
    out.type = static_cast<MsgType>(buffer[3]);
    out.sequence = read_u32(&buffer[4]);
    out.payload.assign(&buffer[HEADER_SIZE], &buffer[HEADER_SIZE + payload_len]);
    return ParseResult::OK;
  }
  const char *parse_result_to_str(ParseResult r)
  {
    switch (r)
    {
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
} // namespace juggernog