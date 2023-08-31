#include "requester.hpp"

#include <unistd.h>

#include <fmt/ranges.h>

void Requester::do_write(RequestData data) {
  // 数据的前两个字节表示本次请求的数据大小，后面的为实际请求数据（明文形式）
  std::string request_body = fmt::format("{}+{}", data.a, data.b);
  auto [_, data_size] =
      fmt::format_to_n(send_buffer.data() + header_size,
                       send_buffer.size() - header_size, "{}", request_body);
  set_header(send_buffer.data(), {static_cast<uint16_t>(data_size)});
  size_t total_size = data_size + header_size;
  int bytes_send;
  do {
    bytes_send = write(sock_fd, send_buffer.data(), total_size);
    total_size -= bytes_send;
  } while (bytes_send > 0 && total_size > 0);
  if (bytes_send == -1) {
    throw write_error(get_errno_string());
  }
}

void Requester::do_request() {
  RequestData request_data{1234, 5678};
  // RequestData request_data{rand() % (1 << 20), rand() % (1 << 20)};
  requests.emplace(request_data);
  do_write(request_data);
}

void Requester::do_read() {

  int bytes_received =
      read(sock_fd, recv_buffer.data() + bytes_written, bytes_available);
  if (bytes_received == 0) {
    throw eof_error("eof");
  } else if (bytes_received == -1) {
    throw read_error(get_errno_string());
  }
  char *data_ptr = recv_buffer.data();
  int packet_count = 0;
  std::queue<std::string_view> responses;
  bytes_received += bytes_written;
  while (bytes_received > 0) {
    if (bytes_received < header_size) {
      // move this chunk to buffer front
      std::copy(data_ptr, data_ptr + bytes_received, recv_buffer.data());
      // INFO("incomplete header");
      break;
    }
    // read packet
    uint16_t data_size = get_header(data_ptr).size;
    size_t total_size = data_size + header_size;
    if (total_size > bytes_received) {
      // not fully read, wait for next read
      std::copy(data_ptr, data_ptr + bytes_received, recv_buffer.data());
      // INFO("data not fully received, wait for next read");
      break;
    }
    bytes_received -= total_size;
    packet_count++;
    std::string_view packet_data(data_ptr + header_size, data_size);
    responses.emplace(packet_data);
    data_ptr += total_size;
  }
  // INFO("responses in one read:", responses);
  while (!responses.empty()) {
    int expected_value = requests.front().a + requests.front().b;
    if (expected_value != stoi(responses.front())) {
      throw program_error("value error, expect {} + {} = {}, got {}",
                          requests.front().a, requests.front().b,
                          expected_value, responses.front());
    }
    // INFO("{} + {} = {}, expected {}", requests.front().a, requests.front().b,
    //      responses.front(), expected_value);
    requests.pop();
    responses.pop();
  }
  bytes_written = bytes_received;
  bytes_available = recv_buffer.size() - bytes_received;
}