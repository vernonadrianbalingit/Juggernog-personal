/* 
server.cpp - UPP echo server
Binds to UDP port 9999, echoes back whatever bytes arrive 

CS 576 mapping : 
  Chapter 1, slide 68 (TCP/DP Sockets Logics) - right column 
  Chapter 5, slide 2 (Simple Demultiplexer - Best Effort UDP)
*/

#include <cstdio> //printf, perror 
#include <cstdlib> //exit 
#include <cstring> //memset 
#include <unistd.h> //close 
#include <arpa/inet.h> //socketaddr_in, htons, inet_ntop
#include <sys/socket.h> //socket, bind, recvfrom, sendto

namespace { 
  constexpr int PORT = 9999; //UDP port number to bind to
  constexpr int BUFF_SIZE = 1024; //size of buffer for receiving data, in bytes
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

  printf("UDP echo is listening on UDP port %d\n", PORT);

  //STEP 4: Loop forever, receiving and echoing back UDP packets
  char buff[BUFF_SIZE]; //buffer for receiving data
  sockaddr_in client_addr{}; //address of the client that sent the packet
  socklen_t client_len = 0; //size of client_addr, in bytes

  for(;;) {
    client_len = sizeof(client_addr); //must be set before recvfrom()

    //recvfrom blocks until a UDP packet arrives at the socket. 
    //It fills in the client_addr struct with the sender's address, and returns the number of bytes received, or -1 on error.
    ssize_t recv_len = recvfrom(sock,
                                buff,
                                BUFF_SIZE - 1, 
                                0, //flags
                                reinterpret_cast<sockaddr*>(&client_addr),
                                &client_len);

    if (recv_len < 0) { 
      perror("recvfrom() failed");
      continue; //try again to receive the next packet
    }

    buff[recv_len] = '\0'; //null-terminated so we can printf the payload 

    //print who sends it. inet_ntop: 32-bit IPv4 address to string
    char client_ip[INET_ADDRSTRLEN]; //buffer for client IP string
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    printf("Received %zd bytes from %s:%d: %s\n",
           recv_len, client_ip, ntohs(client_addr.sin_port), buff);

    //Echo the same bytes back to the sender 
    ssize_t sent = sendto(sock, buff, static_cast<size_t>(recv_len), 0, 
                          reinterpret_cast<sockaddr*>(&client_addr), client_len);
    if (sent < 0) { 
      perror("sendto() failed");
    }
  }

  close(sock); //unreachable, good form 
  return 0;
}