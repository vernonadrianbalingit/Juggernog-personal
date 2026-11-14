/*
client.cpp - Juggernog UDP client with stop and wait reliablity 

usage: ./client<server-ip>[lossrate][count]
  ./client 127.0.0.1        ->one dispense request
  ./client 127.0.0.1 0.4    ->drop 40% of outbound req
  ./client 127.0.0.1 0.4 5  ->5 requests, 40 percent loss 

*/

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "frame.h"
#include "reliable.h"

namespace {
constexpr int PORT = 9999;
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <server-ip> [loss-rate] [count]\n", argv[0]);
    return 1;
  }

  // Optional 2nd arg: drop this fraction of OUTBOUND requests on purpose
  if (argc > 2) {
    juggernog::set_lossrate(atof(argv[2]));
    printf("Simulating %.0f%% request loss\n", juggernog::loss_rate() * 100.0);
  }

  // Optional 3rd arg: how many requests to send (default 1)
  const int count = (argc > 3) ? atoi(argv[3]) : 1;

  // Same socket() call as the server
  const int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket() failed");
    return 1;
  }

  // Describing the SERVER's address and port to send to
  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);

  // inet_pton: parse "127.0.0.1" to a 32 bit binary IP
  if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) {
    fprintf(stderr, "Invalid server IP address: %s\n", argv[1]);
    close(sock);
    return 1;
  }

  // We don't bind here. The kernel will auto assign a random source
  // port when we first send, and run "lsof -i UDP" when exected to see it

  // Defaults: 200ms base timeout, x2 each retry, cap 3200ms, 5 retries
  const juggernog::RetryConfig cfg;
  int succeeded = 0;

  for (int i = 0; i < count; ++i) {
    // Build a DISPENSE_REQ. Seq starts at 1 and goes up by one per request
    // so the server's duplicate filter sees each one as NEW exactly once
    juggernog::Frame req{};
    req.version = juggernog::VERSION;
    req.type = juggernog::MsgType::DISPENSE_REQ;
    req.sequence = static_cast<uint32_t>(i + 1);
    const char* msg = "amount=30,user=adrian";
    req.payload.assign(msg, msg + strlen(msg));

    juggernog::Frame reply{};
    juggernog::SendStats stats{};

    printf("--- seq=%u ---\n", req.sequence);
    // Replaces the old sendto + blocking recvfrom: sends, waits with a
    // timeout, retransmits with backoff until ACKed or out of retries
    const bool ok = juggernog::send_reliable(sock, req, server_addr, cfg,
                                             reply, stats);

    if (ok) {
      succeeded++;
      printf("  ACKed after %d transmission(s)", stats.transmissions);
      if (stats.rtt_ms >= 0) {
        printf(", rtt=%dms\n", stats.rtt_ms);
      } else {
        printf(", rtt=n/a (Karn/Partridge its retransmitted cannot rtt)\n");
      }
    } else {
      printf("  FAILED after %d transmission(s) -- giving up\n",
             stats.transmissions);
    }
  }

  printf("\n%d/%d request(s) delivered.\n", succeeded, count);
  close(sock);
  return 0;
}
