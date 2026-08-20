#include "frame.h"
#include "crc32.h"
#include <cstdio>
#include <cstring>
#include <string>
using namespace juggernog;
namespace
{
  int tests_run = 0;
  int tests_failed = 0;
  void check(bool cond, const char *name)
  {
    ++tests_run;
    if (cond)
    {
      std::printf("PASS: %s\n", name);
    }
    else
    {
      ++tests_failed;
      std::printf("FAIL: %s\n", name);
    }
  }
  Frame make_frame(MsgType type, uint32_t seq, const std::string &payload)
  {
    Frame f;
    f.version = VERSION;
    f.type = type;
    f.sequence = seq;
    f.payload.assign(payload.begin(), payload.end());
    return f;
  }
} // anonymous namespace
int main()
{
  // Round trip: serialize then parse should recover the same frame.
  {
    Frame in = make_frame(MsgType::DISPENSE_REQ, 42, "juggernog");
    std::vector<uint8_t> wire = serialize(in);
    Frame out;
    ParseResult r = parse(wire.data(), wire.size(), out);
    check(r == ParseResult::OK, "round trip parses OK");
    check(out.version == in.version, "round trip version");
    check(out.type == in.type, "round trip type");
    check(out.sequence == in.sequence, "round trip sequence");
    check(out.payload == in.payload, "round trip payload");
  }
  // Empty payload is valid.
  {
    Frame in = make_frame(MsgType::PING, 1, "");
    std::vector<uint8_t> wire = serialize(in);
    check(wire.size() == FRAMING_OVERHEAD, "empty payload wire size");
    Frame out;
    check(parse(wire.data(), wire.size(), out) == ParseResult::OK,
          "empty payload parses OK");
    check(out.payload.empty(), "empty payload stays empty");
  }
  // Too short: fewer bytes than header + CRC.
  {
    Frame out;
    uint8_t tiny[4] = {0};
    check(parse(tiny, sizeof(tiny), out) == ParseResult::TOO_SHORT,
          "short buffer rejected");
  }
  // Bad magic.
  {
    Frame in = make_frame(MsgType::PING, 2, "x");
    std::vector<uint8_t> wire = serialize(in);
    wire[0] ^= 0xFF;
    Frame out;
    check(parse(wire.data(), wire.size(), out) == ParseResult::BAD_MAGIC,
          "bad magic rejected");
  }
  // Bad version.
  {
    Frame in = make_frame(MsgType::PING, 3, "x");
    std::vector<uint8_t> wire = serialize(in);
    wire[2] = VERSION + 1;
    Frame out;
    check(parse(wire.data(), wire.size(), out) == ParseResult::BAD_VERSION,
          "bad version rejected");
  }
  // Length mismatch: declared length doesn't match buffer size.
  {
    Frame in = make_frame(MsgType::PING, 4, "abc");
    std::vector<uint8_t> wire = serialize(in);
    Frame out;
    check(parse(wire.data(), wire.size() - 1, out) == ParseResult::LENGTH_MISMATCH,
          "truncated buffer rejected");
  }
  // Corrupted payload fails CRC.
  {
    Frame in = make_frame(MsgType::STATUS_RES, 5, "status");
    std::vector<uint8_t> wire = serialize(in);
    wire[HEADER_SIZE] ^= 0xFF;
    Frame out;
    check(parse(wire.data(), wire.size(), out) == ParseResult::BAD_CRC,
          "corrupted payload rejected");
  }
  // Corrupted CRC field itself fails CRC.
  {
    Frame in = make_frame(MsgType::PONG, 6, "pong");
    std::vector<uint8_t> wire = serialize(in);
    wire[wire.size() - 1] ^= 0xFF;
    Frame out;
    check(parse(wire.data(), wire.size(), out) == ParseResult::BAD_CRC,
          "corrupted CRC rejected");
  }
  // CRC-32 known-answer test: "123456789" -> 0xCBF43926 (IEEE 802.3 check value).
  {
    const uint8_t msg[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    check(crc32(msg, sizeof(msg)) == 0xCBF43926u, "crc32 known answer");
  }
  std::printf("\n%d/%d tests passed\n", tests_run - tests_failed, tests_run);
  return tests_failed == 0 ? 0 : 1;
}
