#include "client.hpp"
#include "utils/common.hpp"
#include "utils/exceptions.hpp"

void Client::create_socket() {
  CHECK(fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
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

void Client::connect() {
  // connect 对于非阻塞
  int ret =
      ::connect(fd_, SOCKADDR(remote_endpoint_), sizeof(remote_endpoint_));
  if (ret == -1) {
    int _errno = errno;
    if (would_block(_errno) || _errno == EINPROGRESS) {
      throw temporarily_unavailable_error();
    }
    THROW(get_errno_string(_errno));
  }
}

const struct sockaddr_in &Client::local_endpoint() {
  if (local_endpoint_.sin_port == 0) {
    socklen_t size = sizeof(local_endpoint_);
    CHECK(getsockname(fd_, SOCKADDR(local_endpoint_), &size));
  }
  return local_endpoint_;
}