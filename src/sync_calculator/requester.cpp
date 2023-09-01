#include "requester.hpp"

#include <unistd.h>

#include <chrono>

#include <fmt/ranges.h>

void Requester::do_write() {
  // 这里可能还需要调整一下
  // 还是一样，我们从 send_buffer_bytes_written 开始写，一直可以写
  // send_buffer_bytes_available 这么多
  char *data_ptr = send_buffer.data() + send_buffer_bytes_written;
  while (send_buffer_bytes_available > 0 && !requests.empty()) {
    auto request = requests.front();
    requests.pop();
    std::string request_body = fmt::format("{}+{}", request.a, request.b);
    size_t packet_size = header_size + request_body.size();
    if (packet_size > send_buffer_bytes_available) {
      // 留着下次再写吧
      break;
    }
    memcpy(data_ptr + header_size, request_body.data(), request_body.size());
    set_content_size(send_buffer.data(),
                     {static_cast<uint16_t>(request_body.size())});
    data_ptr += packet_size;
    send_buffer_bytes_available -= packet_size;
    send_buffer_bytes_written += packet_size;
  }
  ssize_t bytes_written;
  TIMER_BEGIN("write()")
  CHECK(bytes_written =
            write(sock_fd, send_buffer.data(), send_buffer_bytes_written));
  TIMER_END()
  // 最后调整数据？感觉这里实现一个 ring buffer 会不会好一点？
  // TODO: use ring buffer to make here more efficient ?
  INFO("bytes_written: {}, send_buffer_bytes_written: {}", bytes_written,
       send_buffer_bytes_written);
  std::copy(send_buffer.data() + bytes_written,
            send_buffer.data() + send_buffer_bytes_written, send_buffer.data());
  send_buffer_bytes_available += bytes_written;
  send_buffer_bytes_written -= bytes_written;
}

void Requester::do_request() {
  // RequestData request_data{1234, 5678};
  RequestData request_data{rand() % (1 << 20), rand() % (1 << 20)};
  wait_queue.emplace(request_data);
  requests.emplace(request_data);
}

void Requester::do_read() {
  int bytes_received;
  TIMER_BEGIN("read()")
  bytes_received = read(sock_fd, recv_buffer.data() + recv_buffer_bytes_written,
                        recv_buffer_bytes_available);
  TIMER_END()
  if (bytes_received == 0) {
    throw eof_error("eof");
  } else if (bytes_received == -1) {
    throw recv_error(get_errno_string());
  }
  char *data_ptr = recv_buffer.data();
  int packet_count = 0;
  std::queue<std::string_view> responses;
  bytes_received += recv_buffer_bytes_written;
  while (bytes_received > 0) {
    if (bytes_received < header_size) {
      // move this chunk to buffer front
      std::copy(data_ptr, data_ptr + bytes_received, recv_buffer.data());
      INFO("incomplete header");
      break;
    }
    // read packet
    uint16_t data_size = get_content_size(data_ptr).size;
    size_t total_size = data_size + header_size;
    if (total_size > bytes_received) {
      // not fully read, wait for next read
      std::copy(data_ptr, data_ptr + bytes_received, recv_buffer.data());
      INFO("data not fully received, wait for next read");
      break;
    }
    bytes_received -= total_size;
    packet_count++;
    std::string_view packet_data(data_ptr + header_size, data_size);
    responses.emplace(packet_data);
    data_ptr += total_size;
  }
  INFO("responses in one read:", responses);
  while (!responses.empty()) {
    int expected_value = wait_queue.front().a + wait_queue.front().b;
    if (expected_value != stoi(responses.front())) {
      throw program_error("value error, expect {} + {} = {}, got {}",
                          wait_queue.front().a, wait_queue.front().b,
                          expected_value, responses.front());
    }
    INFO("{} + {} = {}, expected {}", wait_queue.front().a,
         wait_queue.front().b, responses.front(), expected_value);
    wait_queue.pop();
    responses.pop();
  }
  recv_buffer_bytes_written = bytes_received;
  recv_buffer_bytes_available = recv_buffer.size() - bytes_received;
}