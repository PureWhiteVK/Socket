#pragma once

#include "utils/common.hpp"

#include <queue>

class Requester {
public:
  Requester() = default;
  Requester(int sock_fd) : sock_fd(sock_fd) {}

  struct RequestData {
    int a{};
    int b{};
  };

  void do_write();
  void do_request();
  void do_read();
  int handle() const { return sock_fd; }
  bool has_requests() const { return !wait_queue.empty(); }
  int n_requests() const { return wait_queue.size(); }

private:
  int sock_fd{};
  std::array<char, 1024> send_buffer{};
  std::array<char, 1024> recv_buffer{};
  // 待发送的所有请求 ？
  std::queue<RequestData> requests{};
  // 等待服务器返回计算结果的 queue
  std::queue<RequestData> wait_queue{};
  size_t recv_buffer_bytes_written = 0;
  size_t recv_buffer_bytes_available = recv_buffer.size();
  size_t send_buffer_bytes_written = 0;
  size_t send_buffer_bytes_available = send_buffer.size();
};