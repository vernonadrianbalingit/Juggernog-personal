/*
server.cpp - UPP echo server
Binds to UDP port 9999, echoes back whatever bytes arrive

CS 576 mapping :
  Chapter 1, slide 68 (TCP/DP Sockets Logics) - right column
  Chapter 5, slide 2 (Simple Demultiplexer - Best Effort UDP)
*/

/*
 * Phase 2: Pourtocol UDP server using binary framing.
 * Receives a frame, validates it, prints info, sends a PONG back.
 */

#include "frame.h"

#include <cstdio> //printf, perror
#include <cstdlib> //exit
#include <cstring> //memset
#include <unistd.h> //close
#include <arpa/inet.h> //socketaddr_in, htons, inet_ntop
#include <sys/socket.h> //socket, bind, recvfrom, sendto

namespace {
  constexpr int PORT = 9999; //UDP port number to bind to
  constexpr int BUFF_SIZE = 2048; //size of buffer for receiving data, in bytes
}

int main() {
  //STEP 1: Ask the kernel for a UDP socket
  //AF_INET = IPv4, SOCK_DGRAM = UDP, 0 = use default protocol for this socket type
  // Returns a file descriptor (small int) for the socket, or -1 on error

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket() failed");
    return 1;
  }

  //STEP 2: Bind the socket to a local address and port
  // This tells the kernel to send UDP packets arriving at this port to our socket
  // listen on every interface (INADDR_ANY) on port 9999.

  sockaddr_in server_addr{}; //zero-initialized
  server_addr.sin_family = AF_INET; //IPv4
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY); //listen on every interface
  server_addr.sin_port = htons(PORT); //UDP port number to bind to, in network

  //Step 3: Bind teh socket to the address
  // after, UDP packdts arriving at this port will be sent to our socket at port 9999
  if (bind(sock,
           reinterpret_cast<sockaddr*>(&server_addr),
           sizeof(server_addr)) < 0) {
    perror("bind() failed");
    close(sock);
    return 1;
  }

  printf("Pourtocol server listening on UDP port %d...\n", PORT);

  //STEP 4: Loop forever, receiving and echoing back UDP packets
  uint8_t buff[BUFF_SIZE]; //buffer for receiving data
  sockaddr_in client_addr{}; //address of the client that sent the packet
  socklen_t client_len = 0; //size of client_addr, in bytes

  for(;;) {
    client_len = sizeof(client_addr); //must be set before recvfrom()

    //recvfrom blocks until a UDP packet arrives at the socket.
    //It fills in the client_addr struct with the sender's address, and returns the number of bytes received, or -1 on error.
    ssize_t recv_len = recvfrom(sock,
                                buff,
                                BUFF_SIZE,
                                0, //flags
                                reinterpret_cast<sockaddr*>(&client_addr),
                                &client_len);

    if (recv_len < 0) {
      perror("recvfrom() failed");
      continue; //try again to receive the next packet
    }

    //print who sends it. inet_ntop: 32-bit IPv4 address to string
    char client_ip[INET_ADDRSTRLEN]; //buffer for client IP string
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

    // Try to parse the frame
    juggernog::Frame frame{};
    auto result = juggernog::parse(buff, static_cast<size_t>(recv_len), frame);
    if (result != juggernog::ParseResult::OK) {
      printf("Rejected packet from %s:%d (%zd bytes): %s\n",
             client_ip, ntohs(client_addr.sin_port), recv_len,
             juggernog::parse_result_to_str(result));
      continue;
    }

    printf("Frame from %s:%d  type=0x%02X  seq=%u  payload=%zu bytes\n",
           client_ip, ntohs(client_addr.sin_port),
           static_cast<uint8_t>(frame.type),
           frame.sequence,
           frame.payload.size());

    // If it was a PING, send a PONG back with the same sequence number.
    if (frame.type == juggernog::MsgType::PING) {
      juggernog::Frame reply{};
      reply.version  = juggernog::VERSION;
      reply.type     = juggernog::MsgType::PONG;
      reply.sequence = frame.sequence; // echo the seq
      auto out = juggernog::serialize(reply);
      ssize_t sent = sendto(sock, out.data(), out.size(), 0,
                            reinterpret_cast<sockaddr*>(&client_addr), client_len);
      if (sent < 0) {
        perror("sendto() failed");
      }
    }
  }

  close(sock); //unreachable, good form
  return 0;
}
