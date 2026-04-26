/*
* client.cpp — UDP echo client
* Sends one message, waits for the echo, prints it, exits.
*
* Usage: ./client <server-ip> <message>
* Example: ./client 127.0.0.1 "hello pourtocol"
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

namespace {
constexpr int PORT = 9999;
constexpr int BUF_SIZE = 1024;
}

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <server-ip> <message>\n", argv[0]);
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
  const char* message = argv[2];
  size_t message_len = strlen(message);

  ssize_t sent_len = sendto(sock, 
                            message, 
                            message_len, 
                            0, //flags
                            reinterpret_cast<sockaddr*>(&server_addr), 
                            sizeof(server_addr));
  if (sent_len < 0) {
    perror("sendto() failed");
    close(sock);
    return 1;
  }

  //Now wait for the echo from the server- no timeout yet 
  char buf[BUF_SIZE];
  sockaddr_in from_addr{};
  socklen_t from_len = sizeof(from_addr); 

  sent_len = recvfrom(sock, 
                buf, 
                BUF_SIZE - 1, //leave space for null terminator
                0, //flags
                reinterpret_cast<sockaddr*>(&from_addr), 
                &from_len);
  if (sent_len < 0) {
    printf("Server echoed %zd bytes: \"%s\"\n", sent_len, buf);

    close(sock);
    return 0; 
  }
}