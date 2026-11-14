/*
server.cpp - UPP echo server
Binds to UDP port 9999, echoes back whatever bytes arrive

Phase 2 update: no longer echoing raw bytes. Every packet gets run
through parse() from frame.h, bad ones get logged and dropped, and a
PING gets a PONG back with the same seq number

Phase 3 update: duplicate suppression. Every well-formed frame gets an
ACK (PING gets a PONG). Only frames the duplicate filter calls NEW run
the side effect (the "pour"). Duplicates and stale frames are re-ACKed
but not re-executed.

Usage: ./server [ack-loss-rate]
  ./server 0.3  -> silently drop 30% of outbound ACKs
*/

#include <arpa/inet.h>   //socketaddr_in, htons, inet_ntop
#include <sys/socket.h>  //socket, bind, recvfrom, sendto
#include <unistd.h>      //close

#include <cstdio>   //printf, perror
#include <cstdlib>  //exit
#include <cstring>  //memset

#include "frame.h"
#include "dedupe.h"
#include "reliable.h"

namespace {
constexpr int PORT = 9999;       // UDP port number to bind to
constexpr int BUFF_SIZE = 2048;  // size of buffer for receiving data, in bytes

void handle_dispense(const juggernog::Frame& frame) {
  //Phase 5 (esp32 idf) replaces this with a REAL GPIO write to the pump relay wooooooo
  printf("  >>> POURING: %.*s\n", 
          static_cast<int>(frame.payload.size()),
          reinterpret_cast<const char*>(frame.payload.data()));
}
}  // namespace

int main(int argc, char** argv) {
  if (argc > 1) { 
    juggernog::set_lossrate(atof(argv[1]));
    printf("Simulating %.0f%% ACK loss\n", juggernog::loss_rate() * 100.0);
  }

  // STEP 1: Ask the kernel for a UDP socket
  // settings - AF_INET = IPv4, SOCK_DGRAM = UDP, 0 = use default protocol for this socket
  // type
  //  Returns a file descriptor (small int) for the socket, or -1 on error

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket() failed");
    return 1;
  }

  // STEP 2: Bind the socket to a local address and port
  //  It tells the kernel to send UDP packets arriving at this port to our
  //  socket listen on every interface (INADDR_ANY) on port 9999.

  sockaddr_in server_addr{};                        // zero init
  server_addr.sin_family = AF_INET;                 // IPv4
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // listen on every interface
  server_addr.sin_port = htons(PORT);  // UDP port number to bind to, in network

  // Step 3: Bind teh socket to the address
  //  after, UDP packdts arriving at this port will be sent to our socket at
  //  port 9999
  if (bind(sock, reinterpret_cast<sockaddr*>(&server_addr),
           sizeof(server_addr)) < 0) {
    perror("bind() failed");
    close(sock);
    return 1;
  }

  printf("Pourtocol server listening on UDP port %d...\n", PORT);

  // Declared OUTSIDE the loop so it remembers each peer's highest seq
  // across packets. Inside the loop it would reset every time.
  juggernog::DuplicateFilter dedupe;

  // STEP 4: Loop forever, receiving and replying to UDP packets
  uint8_t buff[BUFF_SIZE];    // buffer for receiving data
  sockaddr_in client_addr{};  // address of the client that sent the packet
  socklen_t client_len = 0;   // size of client_addr, in bytes

  for (;;) {
    client_len = sizeof(client_addr);  // must be set before recvfrom()

    // recvfrom blocks until a UDP packet arrives at the socket.
    // It fills in the client_addr struct with the sender's address, and returns
    // the number of bytes received, or -1 on error.
    ssize_t recv_len =
        recvfrom(sock, buff, BUFF_SIZE,
                 0,  // flags
                 reinterpret_cast<sockaddr*>(&client_addr), &client_len);

    if (recv_len < 0) {
      perror("recvfrom() failed");
      continue;  // try again to receive the next packet
    }

    // print who sends it. inet_ntop: 32-bit IPv4 address to string
    char client_ip[INET_ADDRSTRLEN];  // buffer for client IP string
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    const int client_port = ntohs(client_addr.sin_port);

    // STEP 5: validate it. parse() does all the checks (magic, version,
    //  length, CRC). If anything fails print why and go back to waiting,
    //   never reply to garbage because we can't trust its sequence number
    juggernog::Frame frame{};
    auto result = juggernog::parse(buff, static_cast<size_t>(recv_len), frame);
    if (result != juggernog::ParseResult::OK) {
      printf("Rejected packet from %s:%d (%zd bytes): %s\n", client_ip,
             client_port, recv_len, juggernog::parse_result_to_str(result));
      continue;
    }

    // STEP 6: classify it. NEW = first time we've seen this seq from this
    //  peer. DUPLICATE/STALE = a retransmit of something already handled
    const auto verdict = dedupe.check(client_addr, frame.sequence);

    printf("Frame from %s:%d  type=0x%02X  seq=%u  payload=%zu bytes [%s]\n",
           client_ip, client_port, static_cast<uint8_t>(frame.type),
           frame.sequence, frame.payload.size(),
           juggernog::verdict_to_str(verdict));

    // STEP 7: side effects run for NEW frames only. This is the whole point.
    if (verdict == juggernog::DuplicateFilter::Verdict::NEW) {
      if (frame.type == juggernog::MsgType::DISPENSE_REQ) {
        handle_dispense(frame);
      }
    } else {
      printf("  (suppressed -- already handled seq=%u)\n", frame.sequence);
    }

    // STEP 8: ACK unconditionally, including duplicates. If we stayed silent,
    //  the sender would retransmit until it exhausted its retry budget.
    juggernog::Frame ack{};
    ack.version = juggernog::VERSION;
    ack.type = (frame.type == juggernog::MsgType::PING)
                   ? juggernog::MsgType::PONG
                   : juggernog::MsgType::ACK;
    ack.sequence = frame.sequence;
    const auto out = juggernog::serialize(ack);
    // lossy_sendto instead of sendto can drop the ACK on purpose if a
    // loss rate was set on the command line
    juggernog::lossy_sendto(sock, out.data(), out.size(), client_addr);
  }

  close(sock);  // unreachable, jic just in case 
  return 0;
}
