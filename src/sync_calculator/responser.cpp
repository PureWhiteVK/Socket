#include "responser.hpp"

#include <unistd.h>

#include <fmt/ranges.h>

#include "CalculatorLexer.h"
#include "CalculatorParser.h"
#include "antlr4-runtime.h"

namespace ch = std::chrono;

void Responser::do_write() {
  while (!responses.empty()) {
    auto result = responses.front();
    responses.pop();
    auto [_, data_size] =
        fmt::format_to_n(send_buffer.data() + header_size,
                         send_buffer.size() - header_size, "{}", result);
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
}

void Responser::do_response(std::string_view request_data) {
  try {
    antlr4::ANTLRInputStream inputs(request_data);
    parser::CalculatorLexer lexer(&inputs);
    antlr4::CommonTokenStream tokens(&lexer);
    parser::CalculatorParser parser(&tokens);
    parser.setBuildParseTree(false);
    parser.s();
    // TODO: add error handler here
    responses.emplace(parser.expression_value);
  } catch (std::exception &err) {
    INFO("parse error: {}", err.what());
    THROW("{}", err.what());
  }
}

void Responser::do_read() {
  ch::steady_clock::time_point start = ch::steady_clock::now();
  int bytes_received = read(sock_fd, recv_buffer.data() + bytes_written, bytes_available);
  ch::steady_clock::time_point end = ch::steady_clock::now();
  // INFO("read() takes: {:.3f}ms",
  //      ch::duration_cast<ch::microseconds>(end - start).count() / 1000.0f);
  if (bytes_received == 0) {
    throw eof_error("eof");
  } else if (bytes_received == -1) {
    throw read_error(get_errno_string());
  }
  char *data_ptr = recv_buffer.data();
  int packet_count = 0;
  std::queue<std::string_view> requests;
  // 需要算上之前的一起读取
  bytes_received += bytes_written;
  // INFO("bytes_received: {}",bytes_received);
  // 这里实际上还有上次残留的 offset 没有算上，所以计算有错误，导致后面的读取全乱了
  while (bytes_received > 0) {
    if(bytes_received < header_size) {
      // move this chunk to buffer front
      std::copy(data_ptr, data_ptr + bytes_received, recv_buffer.data());
      // INFO("incomplete header, bytes_received: {}",bytes_received);
      break;
    }
    // 此处有可能 bytes_received == 1，甚至都读不到实际的 header，这种情况怎么办？
    // read packet
    uint16_t data_size = get_header(data_ptr).size;
    size_t total_size = data_size + header_size;
    if (total_size > bytes_received) {
      // we have to this part of data to the front of buffer
      // If the objects overlap, the behavior is undefined.
      // check for overlap
      // overlapped
      std::copy(data_ptr, data_ptr + bytes_received, recv_buffer.data());
      // not fully read, wait for next read
      // INFO("incomplete body");
      break;
    }
    std::string_view packet_data(data_ptr + header_size, data_size);
    requests.emplace(packet_data);
    do_response(packet_data);

    packet_count++;
    data_ptr += total_size;
    bytes_received -= total_size;
    // INFO("request [{}]: {}", packet_count, packet_data);
    // 此处一定是按顺序处理的，因为 数据包是按顺序发送的
  }
  // INFO("requests in one read:");
  // while (!requests.empty()) {
  //   INFO("{}", requests.front());
  //   requests.pop();
  // }
  // 此处 offset 怎么会等于 1024 ？？
  bytes_written = bytes_received;
  INFO("offset = {}",bytes_written);
  bytes_available = recv_buffer.size() - bytes_received;
}