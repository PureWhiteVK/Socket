#pragma once

#include "utils/common.hpp"

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
  std::queue<int> responses{};
  size_t send_buffer_bytes_written = 0;
  size_t send_buffer_bytes_available = send_buffer.size();
  size_t recv_buffer_bytes_written = 0;
  size_t recv_buffer_bytes_available = recv_buffer.size();
};