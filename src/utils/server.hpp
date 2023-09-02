#pragma once

#include "common.hpp"
#include "exceptions.hpp"

#include <arpa/inet.h>
#include <functional>

class Session {
public:
  Session() = default;
  int handle() const { return fd_; }
  const struct sockaddr_in &remote_endpoint() { return remote_endpoint_; }
  const struct sockaddr_in &local_endpoint() { return local_endpoint_; }

  friend class Server;
  friend struct std::hash<Session>;
  bool operator==(const Session &b) const noexcept {
    return fd_ == b.fd_ && remote_endpoint_ == b.remote_endpoint_ &&
           local_endpoint_ == b.local_endpoint_;
  }

protected:
  int fd_{};
  struct sockaddr_in remote_endpoint_ {};
  struct sockaddr_in local_endpoint_ {};
};

namespace std {
template <> struct hash<Session> {
  size_t operator()(const Session &s) const noexcept {
    return std::hash<int>()(s.fd_);
  }
};
}; // namespace std

class Server {
public:
  Server(std::string_view ip, uint16_t port);

  Server(uint16_t port);

  Server &bind();
  Server &listen(int backlog_size);

  int handle() const { return fd_; }

  const struct sockaddr_in &local_endpoint() const { return local_endpoint_; }

  Session accept();

private:
  void create_socket();

private:
  int fd_{};
  struct sockaddr_in local_endpoint_ {};
};