#include "reliable.h"
#include "dedupe.h"

#include <thread> 
#include <arpa/inet.h>
#include <sys/socket.h> // inet , htons
#include <netinet/in.h>
#include <unistd.h> 

#include <cstdio>
#include <cstring>
#include <string>

#include "crc32.h"
#include "frame.h"

using namespace juggernog;
namespace {
int tests_run = 0;
int tests_failed = 0;
void check(bool cond, const char* name) {
  ++tests_run;
  if (cond) {
    std::printf("PASS: %s\n", name);
  } else {
    ++tests_failed;
    std::printf("FAIL: %s\n", name);
  }
}
Frame make_frame(MsgType type, uint32_t seq, const std::string& payload) {
  Frame f;
  f.version = VERSION;
  f.type = type;
  f.sequence = seq;
  f.payload.assign(payload.begin(), payload.end());
  return f;
}

void test_backoff_doubles_and_caps() { 
  RetryConfig config;
  config.initial_timeout_ms = 200;
  config.max_timeout_ms = 3200;

  check(next_timeout_ms(200, config) == 400, "backoff 200 -> 400");
  check(next_timeout_ms(400, config) == 800, "backoff 400 -> 800");
  check(next_timeout_ms(800, config) == 1600, "backoff 800 -> 1600");
  check(next_timeout_ms(1600, config) == 3200, "backoff 1600 -> 3200");
  check(next_timeout_ms(3200, config) == 3200, "CLAMPED, NOT 6400");
}

void test_jitter_stays_in_bounds() { 
  bool all_in_bounds = true;

  for (int i = 0; i < 1000; ++i) { 
    const int j = apply_jitter(200, 0.25);
    if (j < 150 || j > 250) {
      all_in_bounds = false;
      break; //one bad value is enough 
    }
  }
  check(all_in_bounds, "jitter 200ms +/- 25% stays in [150, 250]");

  //zero jitter should give exact value too - timing tests needs that 
  check(apply_jitter(200, 0.0) == 200, "zero jitter returns exact timeout");
}

sockaddr_in make_peer(const char* ip, uint16_t port) { 
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  inet_pton(AF_INET, ip, &a.sin_addr);
  return a; 
} 

void test_duplicate_filter_basic() {
  DuplicateFilter f;
  const sockaddr_in peer = make_peer("192.168.1.50", 40000);

  check(f.check(peer, 1) == DuplicateFilter::Verdict::NEW,
        "dedupe: first seq 1 is NEW");
  check(f.check(peer, 1) == DuplicateFilter::Verdict::DUPLICATE,
        "dedupe: repeated seq 1 is DUPLICATE");
  check(f.check(peer, 2) == DuplicateFilter::Verdict::NEW,
        "dedupe: higher seq 2 is NEW");
  check(f.check(peer, 1) == DuplicateFilter::Verdict::STALE,
        "dedupe: older seq 1 after 2 is STALE");
  check(f.check(peer, 2) == DuplicateFilter::Verdict::DUPLICATE,
        "dedupe: repeated seq 2 is DUPLICATE");
}

void test_duplicate_filter_separates_peers() {
  DuplicateFilter f;
  const sockaddr_in a = make_peer("192.168.1.50", 40000);
  const sockaddr_in b = make_peer("192.168.1.51", 40000);  // different host
  const sockaddr_in c = make_peer("192.168.1.50", 40001);  // same IP, different port

  check(f.check(a, 5) == DuplicateFilter::Verdict::NEW,
        "dedupe: peer A seq 5 is NEW");
  check(f.check(b, 5) == DuplicateFilter::Verdict::NEW,
        "dedupe: peer B seq 5 is NEW (different IP)");
  check(f.check(c, 5) == DuplicateFilter::Verdict::NEW,
        "dedupe: peer C seq 5 is NEW (same IP, different port)");
  check(f.check(a, 5) == DuplicateFilter::Verdict::DUPLICATE,
        "dedupe: peer A seq 5 again is DUPLICATE");
  check(f.tracked_peers() == 3, "dedupe: tracks 3 separate peers");
}

void test_reliable_retransmits_until_acked() { 
  //Responder socket on a temp short lived port 
  const int srv = socket(AF_INET, SOCK_DGRAM, 0);
  check(srv >= 0, "retransmit test: responder socket created");

  sockaddr_in srv_addr{};
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  srv_addr.sin_port = 0; //kernel picks a free port
  check(bind(srv, reinterpret_cast<sockaddr*>(&srv_addr), sizeof(srv_addr)) == 0,
         "retransmit test: responder bound to loopback");

  socklen_t addr_len = sizeof(srv_addr);
  check(getsockname(srv, reinterpret_cast<sockaddr*>(&srv_addr),
                        &addr_len) == 0, "retransmit test: getsockname reports assigned port");

  constexpr int DROPS = 2; //ignore the first 2, ACK the third. 

  // Responder thread: plays the "other machine". Stays silent for the first
  // DROPS requests (simulated loss), then ACKs the next one.
  std::thread responder([srv]() {
    for (int i = 0; i < DROPS + 1; ++i) {
      uint8_t buf[2048];
      sockaddr_in from{};
      socklen_t from_len = sizeof(from);

      const ssize_t n = recvfrom(srv, buf, sizeof(buf), 0,
                                 reinterpret_cast<sockaddr*>(&from), &from_len);
      if (n < 0) return;
      if (i < DROPS) continue;  // deliberate silence -> sender must retransmit

      Frame req{};
      if (parse(buf, static_cast<size_t>(n), req) != ParseResult::OK) return;

      Frame ack{};
      ack.version = VERSION;
      ack.type = MsgType::ACK;
      ack.sequence = req.sequence;  // echo the seq so the sender knows it's ours
      const std::vector<uint8_t> out = serialize(ack);
      sendto(srv, out.data(), out.size(), 0,
             reinterpret_cast<sockaddr*>(&from), from_len);
    }
  });

  // Client side: never bound, so the kernel gives it an ephemeral port on
  // the first send.
  const int cli = socket(AF_INET, SOCK_DGRAM, 0);
  check(cli >= 0, "retransmit test: client socket created");

  Frame req{};
  req.version = VERSION;
  req.type = MsgType::PING;
  req.sequence = 7;

  RetryConfig cfg;
  cfg.initial_timeout_ms = 50;  // keep the test fast
  cfg.jitter = 0.0;             // deterministic timing
  cfg.max_retries = 5;

  Frame reply{};
  SendStats stats{};
  const bool ok = send_reliable(cli, req, srv_addr, cfg, reply, stats);

  check(ok, "retransmit test: send_reliable succeeds");
  check(stats.transmissions == DROPS + 1,
        "retransmit test: sent 3 times (2 dropped + 1 ACKed)");
  check(stats.acked, "retransmit test: stats report acked");
  check(reply.sequence == 7, "retransmit test: reply echoes seq 7");
  check(reply.type == MsgType::ACK, "retransmit test: reply is an ACK");
  // Karn/Partridge: an ACK after a retransmission is ambiguous, so no RTT sample
  check(stats.rtt_ms == -1, "retransmit test: no RTT sample after retransmit");

  responder.join();  // wait for the responder before closing its socket
  close(cli);
  close(srv);
}

void test_reliable_gives_up_when_peer_is_silent() {
  // Bound socket that nobody ever reads from: every request goes unanswered.
  const int srv = socket(AF_INET, SOCK_DGRAM, 0);
  check(srv >= 0, "give-up test: silent socket created");

  sockaddr_in srv_addr{};
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  srv_addr.sin_port = 0;  // kernel picks a free port
  check(bind(srv, reinterpret_cast<sockaddr*>(&srv_addr), sizeof(srv_addr)) == 0,
        "give-up test: silent socket bound to loopback");

  socklen_t addr_len = sizeof(srv_addr);
  check(getsockname(srv, reinterpret_cast<sockaddr*>(&srv_addr), &addr_len) == 0,
        "give-up test: getsockname reports assigned port");

  const int cli = socket(AF_INET, SOCK_DGRAM, 0);
  check(cli >= 0, "give-up test: client socket created");

  Frame req{};
  req.version = VERSION;
  req.type = MsgType::PING;
  req.sequence = 99;

  RetryConfig cfg;
  cfg.initial_timeout_ms = 10;  // 10 + 20 + 40 + 40 = ~110ms total
  cfg.max_timeout_ms = 40;
  cfg.jitter = 0.0;             // deterministic timing
  cfg.max_retries = 3;

  Frame reply{};
  SendStats stats{};
  check(send_reliable(cli, req, srv_addr, cfg, reply, stats) == false,
        "give-up test: send_reliable returns false");
  check(stats.transmissions == 4, "give-up test: 1 initial + 3 retries = 4 sends");
  check(stats.acked == false, "give-up test: stats report not acked");

  close(cli);
  close(srv);
}


}  // anonymous namespace
int main() {
  // serialize then parse should hand back exactly what we put in
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
  // PING with no payload is legal (thats what the client sends with no
  // message arg). Wire size should be just header + CRC
  {
    Frame in = make_frame(MsgType::PING, 1, "");
    std::vector<uint8_t> wire = serialize(in);
    check(wire.size() == FRAMING_OVERHEAD, "empty payload wire size");
    Frame out;
    check(parse(wire.data(), wire.size(), out) == ParseResult::OK,
          "empty payload parses OK");
    check(out.payload.empty(), "empty payload stays empty");
  }
  // 4 bytes isn't even a full header, parse should bail before reading
  // any fields
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
    check(parse(wire.data(), wire.size() - 1, out) ==
              ParseResult::LENGTH_MISMATCH,
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
  // CRC-32 known-answer test: "123456789" -> 0xCBF43926 (IEEE 802.3 check
  // value).
  {
    const uint8_t msg[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    check(crc32(msg, sizeof(msg)) == 0xCBF43926u, "crc32 known answer");
  }
  // Phase 3
  test_backoff_doubles_and_caps();
  test_jitter_stays_in_bounds();
  test_duplicate_filter_basic();
  test_duplicate_filter_separates_peers();
  test_reliable_retransmits_until_acked();
  test_reliable_gives_up_when_peer_is_silent();

  std::printf("\n%d/%d tests passed\n", tests_run - tests_failed, tests_run);
  return tests_failed == 0 ? 0 : 1;
}
