#include "server.hpp"
#include "utils/common.hpp"

void Server::create_socket() {
  CHECK(fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
}

Server::Server(std::string_view ip, uint16_t port) {
  local_endpoint_.sin_family = AF_INET;
  local_endpoint_.sin_port = ::htons(port);
  set_addr(local_endpoint_, ip);
  create_socket();
}

Server::Server(uint16_t port) {
  local_endpoint_.sin_family = AF_INET;
  local_endpoint_.sin_port = ::htons(port);
  local_endpoint_.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
  create_socket();
}

Server &Server::bind() {
  CHECK(::bind(fd_, SOCKADDR(local_endpoint_), sizeof(local_endpoint_)));
  return *this;
}

Server &Server::listen(int backlog_size) {
  CHECK(::listen(fd_, backlog_size));
  INFO("start server at: {}", local_endpoint_);
  return *this;
}

Session Server::accept() {
  Session sess = Session();
  socklen_t size = sizeof(sess.remote_endpoint_);
  sess.fd_ = ::accept(fd_, SOCKADDR(sess.remote_endpoint_), &size);
  if (sess.fd_ == -1) {
    int _errno = errno;
    if (would_block(_errno) || _errno == EINPROGRESS) {
      throw temporarily_unavailable_error();
    }
    THROW(get_errno_string(_errno));
  }
  ::memcpy(&sess.local_endpoint_, &local_endpoint_, sizeof(local_endpoint_));
  DEBUG("accept connection from {}", sess.remote_endpoint_);
  return sess;
}