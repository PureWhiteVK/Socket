#pragma once

#include "util.hpp"

#include <queue>

class Requester {
public:
  Requester() = default;
  Requester(int sock_fd) : sock_fd(sock_fd) {}

  struct RequestData {
    int a{};
    int b{};
  };

  void do_write(RequestData data);
  void do_request();
  void do_read();
  int handle() const { return sock_fd; }
  bool has_requests() const { return !requests.empty(); }
  int n_requests() const { return requests.size(); }

private:
  int sock_fd{};
  std::array<char, 1024> send_buffer{};
  std::array<char, 1024> recv_buffer{};
  std::queue<RequestData> requests{};
  size_t bytes_written = 0;
  size_t bytes_available = recv_buffer.size();
};