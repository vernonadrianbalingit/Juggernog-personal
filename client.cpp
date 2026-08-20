/*
* client.cpp — UDP echo client
* Sends one message, waits for the echo, prints it, exits.
*
* Usage: ./client <server-ip> <message>
* Example: ./client 127.0.0.1 "hello pourtocol"
*/

/*
 * Phase 2: Pourtocol UDP client using binary framing.
 * Sends a PING frame (optionally carrying <message> as its payload),
 * waits for PONG, prints the result.
 */
#include "frame.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

namespace {
constexpr int PORT = 9999;
constexpr int BUF_SIZE = 2048;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <server-ip> [message]\n", argv[0]);
    return 1;
  }

  //Same socket() call as the server - UDP is symmetric
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket() failed");
    return 1;
  }

  //Describe the SERVER's address and port to send to
  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);


  //inet_pton: parse "127.0.0.1" to a 32 bit binary IP
  if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) {
    fprintf(stderr, "Invalid server IP address: %s\n", argv[1]);
    close(sock);
    return 1;
  }

  //We don't bind here. The kernel will auto assign a random source
  //port when we first send, and run "lsof -i UDP" when exected to see it

  // Build a PING frame
  juggernog::Frame ping{};
  ping.version  = juggernog::VERSION;
  ping.type     = juggernog::MsgType::PING;
  ping.sequence = 1;
  if (argc >= 3) {
    const char* message = argv[2];
    size_t message_len = strlen(message);
    if (message_len > juggernog::MAX_PAYLOAD) {
      fprintf(stderr, "Message too long (max %zu bytes)\n", juggernog::MAX_PAYLOAD);
      close(sock);
      return 1;
    }
    ping.payload.assign(message, message + message_len);
  }

  auto out = juggernog::serialize(ping);
  printf("Sending PING (%zu bytes on the wire)...\n", out.size());

  ssize_t sent_len = sendto(sock,
                            out.data(),
                            out.size(),
                            0, //flags
                            reinterpret_cast<sockaddr*>(&server_addr),
                            sizeof(server_addr));
  if (sent_len < 0) {
    perror("sendto() failed");
    close(sock);
    return 1;
  }

  //Now wait for the echo from the server- no timeout yet
  uint8_t buf[BUF_SIZE];
  sockaddr_in from_addr{};
  socklen_t from_len = sizeof(from_addr);

  ssize_t n = recvfrom(sock,
                buf,
                BUF_SIZE,
                0, //flags
                reinterpret_cast<sockaddr*>(&from_addr),
                &from_len);
  if (n < 0) {
    perror("recvfrom() failed");
    close(sock);
    return 1;
  }

  juggernog::Frame reply{};
  auto result = juggernog::parse(buf, static_cast<size_t>(n), reply);
  if (result != juggernog::ParseResult::OK) {
    fprintf(stderr, "Got malformed reply: %s\n",
            juggernog::parse_result_to_str(result));
    close(sock);
    return 1;
  }

  printf("Got reply: type=0x%02X seq=%u payload=%zu bytes\n",
         static_cast<uint8_t>(reply.type),
         reply.sequence,
         reply.payload.size());

  close(sock);
  return 0;
}
