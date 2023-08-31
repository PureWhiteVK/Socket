/*
Example developed and tested on Ubuntu Linux 18.04.2.

This example shows how to trigger select to return immediately by
sending an OOB message to the receiving party.

Works on Linux and Android
The equivalent API code also works on Windows except that Windows Firewall is
triggered.

Enjoy!

chuacw,
14 May 2019,
Singapore.

*/
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <iostream>
#include <string_view>

int main(int argc, char *argv[]) {
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in server, listenaddr;
  bzero((char *)&server, sizeof(server));

  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_family = AF_INET;
  // using random port
  server.sin_port = htons(0);

  int rc = bind(serverSocket, (struct sockaddr *)&server, sizeof(server));
  if (rc >= 0) {
    printf("Server socket bound.\n");
  } else {
    printf("Failed to bind server socket.\n");
  }

  rc = listen(serverSocket, 1);
  if (rc >= 0) {
    printf("Server socket listening.\n");
  } else {
    printf("Failed to listen on server socket.\n");
  }

  socklen_t len = sizeof(listenaddr);
  rc = getsockname(serverSocket, (struct sockaddr *)&listenaddr, &len);
  std::cout << "server port: " << ntohs(listenaddr.sin_port) << std::endl;
  int myPort = ntohs(listenaddr.sin_port);
  if (rc >= 0) {
    printf("Got socket connection address.\n");
  }

  if (listenaddr.sin_addr.s_addr == INADDR_ANY) {
    listenaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  }

  rc = connect(clientSocket, (struct sockaddr *)&listenaddr, len);
  if (rc >= 0) {
    printf("Socket connected.\n");
  }

  int accepted = accept(serverSocket, (struct sockaddr *)&listenaddr, &len);
  if (accepted > 0) {
    printf("Accepted connection!\n");
  }

  std::string_view oob_data = "this is out-of-band data";
  std::string_view normal_data = "this is normal data";

  // Without this, the call to select will timeout.
  send(clientSocket, normal_data.data(), normal_data.size(), 0);
  send(clientSocket, oob_data.data(), oob_data.size(), MSG_OOB);

  fd_set except_fds, read_fds;
  FD_ZERO(&except_fds);
  FD_ZERO(&read_fds);
  FD_SET(accepted, &except_fds);
  FD_SET(accepted, &read_fds);

  printf("Entering select...\n");

  int max_fd = accepted;
  timeval timeout;
  timeout.tv_sec =
      10 * 60; // 10 minutes, but this will return almost immediately.
  timeout.tv_usec = 0;
  rc = select(max_fd + 1, &read_fds, NULL, &except_fds, &timeout);

  switch (rc) {
  case -1:
    printf("Error occurred during select.\n");
    break;
  case 0:
    printf("Timeout occurred during select.\n");
    break;
  default:
    printf("Conditions triggered during select.\n");
  }

  std::cout << "#triggered fds = " << rc << std::endl;

  std::array<char, 1024> buffer;

  size_t bytes_received;
  bytes_received = recv(accepted, buffer.data(), buffer.size(), MSG_OOB);
  std::string_view data_oob(buffer.data(), bytes_received);
  std::cout << "out-of-band data size: " << bytes_received << std::endl;
  std::cout << data_oob << std::endl;

  bytes_received = recv(accepted, buffer.data(), buffer.size(), 0);
  std::string_view data_normal(buffer.data(), bytes_received);
  std::cout << data_normal << std::endl;

  // can we read out-of-band data here ?
  shutdown(accepted, SHUT_RDWR);
  shutdown(clientSocket, SHUT_RDWR);
  
  close(accepted);
  close(clientSocket);
  
  close(serverSocket);

  // if (accepted > 0)
  //   close(accepted);
  // if (clientSocket > 0)
  //   close(clientSocket);
  // if (serverSocket > 0)
  //   close(serverSocket);

  return 0;
}