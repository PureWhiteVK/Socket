#include "responser.hpp"

#include <unistd.h>

#include <fmt/ranges.h>

#include "CalculatorLexer.h"
#include "CalculatorParser.h"
#include "antlr4-runtime.h"

void Responser::do_write() {
  // 这里可能还需要调整一下
  // 还是一样，我们从 send_buffer_bytes_written 开始写，一直可以写
  // send_buffer_bytes_available 这么多
  char *data_ptr = send_buffer.data() + send_buffer_bytes_written;
  while (send_buffer_bytes_available > 0 && !responses.empty()) {
    auto response = responses.front();
    responses.pop();
    std::string response_body = fmt::format("{}", response);
    size_t packet_size = header_size + response_body.size();
    if (packet_size > send_buffer_bytes_available) {
      // 留着下次再写吧
      break;
    }
    memcpy(data_ptr + header_size, response_body.data(), response_body.size());
    set_content_size(send_buffer.data(),
                     {static_cast<uint16_t>(response_body.size())});
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
  std::queue<std::string_view> requests;
  // 需要算上之前的一起读取
  bytes_received += recv_buffer_bytes_written;
  // INFO("bytes_received: {}",bytes_received);
  // 这里实际上还有上次残留的 offset
  // 没有算上，所以计算有错误，导致后面的读取全乱了
  while (bytes_received > 0) {
    if (bytes_received < header_size) {
      // move this chunk to buffer front
      std::copy(data_ptr, data_ptr + bytes_received, recv_buffer.data());
      // INFO("incomplete header, bytes_received: {}",bytes_received);
      break;
    }
    // 此处有可能 bytes_received == 1，甚至都读不到实际的
    // header，这种情况怎么办？ read packet
    uint16_t data_size = get_content_size(data_ptr).size;
    size_t packet_size = data_size + header_size;
    if (packet_size > bytes_received) {
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

    data_ptr += packet_size;
    bytes_received -= packet_size;
    // 此处一定是按顺序处理的，因为 数据包是按顺序发送的
  }
  INFO("requests in one read:");
  while (!requests.empty()) {
    INFO("{}", requests.front());
    requests.pop();
  }
  // 此处 offset 怎么会等于 1024 ？？
  recv_buffer_bytes_written = bytes_received;
  INFO("offset = {}", recv_buffer_bytes_written);
  recv_buffer_bytes_available = recv_buffer.size() - bytes_received;
}