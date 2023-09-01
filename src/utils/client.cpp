#include "client.hpp"

void Client::create_socket() {
  fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  CHECK(fd_);
  socklen_t size = sizeof(local_endpoint_);
  CHECK(getsockname(fd_, SOCKADDR(local_endpoint_), &size));
}

Client::Client(std::string_view remote_ip, uint16_t remote_port) {
  remote_endpoint_.sin_family = AF_INET;
  remote_endpoint_.sin_port = ::htons(remote_port);
  set_addr(remote_endpoint_, remote_ip);
  create_socket();
}

Client::Client(uint16_t remote_port) {
  remote_endpoint_.sin_family = AF_INET;
  remote_endpoint_.sin_port = ::htons(remote_port);
  remote_endpoint_.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
  create_socket();
}

Client &Client::connect() {
  CHECK(::connect(fd_, SOCKADDR(remote_endpoint_), sizeof(remote_endpoint_)));
  INFO("connect to remote endpoint: {}", remote_endpoint_);
  return *this;
}