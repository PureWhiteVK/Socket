#pragma once

#include "util.hpp"

#include <queue>

class Responser {
public:
  Responser() = default;
  Responser(int sock_fd) : sock_fd(sock_fd) {}

  void set_sock_fd(int _sock_fd) { sock_fd = _sock_fd; }
  void do_write();
  void do_response(std::string_view request_data);
  void do_read();
  int handle() const { return sock_fd; }

private:
  int sock_fd{};
  std::array<char, 1024> send_buffer{};
  std::array<char, 1024> recv_buffer{};
  size_t bytes_written = 0;
  size_t bytes_available = recv_buffer.size();
  std::queue<int> responses{};
};