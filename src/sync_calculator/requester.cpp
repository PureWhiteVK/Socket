#include "requester.hpp"
#include "utils/common.hpp"
#include "utils/exceptions.hpp"

#include <cerrno>
#include <string_view>
#include <unistd.h>

#include <chrono>
#include <random>

#include <spdlog/fmt/bundled/ranges.h>

void Requester::do_write() {
  if (requests.empty()) {
    return;
  }
  // 这里可能还需要调整一下
  // 还是一样，我们从 send_buffer_bytes_written 开始写，一直可以写
  // send_buffer_bytes_available 这么多
  char *data_ptr = send_buffer.data() + send_buffer_bytes_written;
  while (send_buffer_bytes_available > 0 && !requests.empty()) {
    auto request = requests.front();
    std::string request_body = fmt::format("{}+{}", request.a, request.b);
    size_t packet_size = header_size + request_body.size();
    if (packet_size > send_buffer_bytes_available) {
      // 留着下次再写吧
      break;
    }
    requests.pop();
    memcpy(data_ptr + header_size, request_body.data(), request_body.size());
    set_content_size(data_ptr, {static_cast<uint16_t>(request_body.size())});
    data_ptr += packet_size;
    send_buffer_bytes_available -= packet_size;
    send_buffer_bytes_written += packet_size;
  }
  ssize_t bytes_written =
      write(sock_fd, send_buffer.data(), send_buffer_bytes_written);
  // 检查返回值，是否是 EAGAIN or EWOULDBLOCK
  if (bytes_written == -1) {
    // ignore
    int _errno = errno;
    if (would_block(errno)) {
      return;
    }
    throw send_error(get_errno_string(_errno));
  }
  DEBUG("send: {}",
        escaped(std::string_view(send_buffer.data(), bytes_written)));
  // 最后调整数据？感觉这里实现一个 ring buffer 会不会好一点？
  // TODO: use ring buffer to make here more efficient ?
  std::copy(send_buffer.data() + bytes_written,
            send_buffer.data() + send_buffer_bytes_written, send_buffer.data());
  send_buffer_bytes_available += bytes_written;
  send_buffer_bytes_written -= bytes_written;
}

void Requester::do_request() {
  static std::mt19937 rand(0);
  static std::uniform_int_distribution<int> dist(0, 1 << 20);
  // RequestData request_data{1234, 5678};
  RequestData request_data{dist(rand), dist(rand)};
  DEBUG("request: {}+{}=?", request_data.a, request_data.b);
  wait_queue.emplace(request_data);
  requests.emplace(request_data);
}

void Requester::do_read() {
  if (wait_queue.empty()) {
    return;
  }
  int bytes_received;
  bytes_received = read(sock_fd, recv_buffer.data() + recv_buffer_bytes_written,
                        recv_buffer_bytes_available);
  if (bytes_received == 0) {
    throw eof_error();
  } else if (bytes_received == -1) {
    int _errno = errno;
    if (would_block(errno)) {
      return;
    }
    throw recv_error(get_errno_string(errno));
  }
  DEBUG("recv: {}",
        escaped(std::string_view(recv_buffer.data() + recv_buffer_bytes_written,
                                 bytes_received)));
  char *data_ptr = recv_buffer.data();
  bytes_received += recv_buffer_bytes_written;
  while (bytes_received > 0) {
    if (bytes_received < header_size) {
      // move this chunk to buffer front
      std::copy(data_ptr, data_ptr + bytes_received, recv_buffer.data());
      // INFO("incomplete header");
      break;
    }
    // read packet
    uint16_t data_size = get_content_size(data_ptr).size;
    size_t total_size = data_size + header_size;
    if (total_size > bytes_received) {
      // not fully read, wait for next read
      std::copy(data_ptr, data_ptr + bytes_received, recv_buffer.data());
      // INFO("data not fully received, wait for next read");
      break;
    }
    bytes_received -= total_size;
    std::string_view response_body(data_ptr + header_size, data_size);
    int expected_value = wait_queue.front().a + wait_queue.front().b;
    int actual_value = stoi(response_body);
    if (expected_value != actual_value) {
      throw program_error("value error, expect {} + {} = {}, got {}",
                          wait_queue.front().a, wait_queue.front().b,
                          expected_value, actual_value);
    }
    wait_queue.pop();
    data_ptr += total_size;
  }
  recv_buffer_bytes_written = bytes_received;
  recv_buffer_bytes_available = recv_buffer.size() - bytes_received;
}