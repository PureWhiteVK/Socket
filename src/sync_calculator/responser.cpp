#include "responser.hpp"

#include <cerrno>
#include <memory>
#include <unistd.h>

#include <spdlog/fmt/bundled/ranges.h>

#include "ANTLRErrorStrategy.h"
#include "BailErrorStrategy.h"
#include "CalculatorLexer.h"
#include "CalculatorParser.h"
#include "antlr4-runtime.h"
#include "utils/common.hpp"

void Responser::do_write() {
  if (responses.empty()) {
    return;
  }
  char *data_ptr = send_buffer.data() + send_buffer_bytes_written;
  while (send_buffer_bytes_available > 0 && !responses.empty()) {
    auto response = responses.front();
    std::string response_body = fmt::format("{}", response);
    size_t packet_size = header_size + response_body.size();
    if (packet_size > send_buffer_bytes_available) {
      break;
    }
    responses.pop();
    memcpy(data_ptr + header_size, response_body.data(), response_body.size());
    set_content_size(data_ptr, {static_cast<uint16_t>(response_body.size())});
    data_ptr += packet_size;
    send_buffer_bytes_available -= packet_size;
    send_buffer_bytes_written += packet_size;
  }
  ssize_t bytes_written =
      write(sock_fd, send_buffer.data(), send_buffer_bytes_written);
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

void Responser::do_response(std::string_view request_data) {
  static parser::CalculatorLexer lexer(nullptr);
  static parser::CalculatorParser parser(nullptr);
  static auto error_handler = std::make_shared<antlr4::BailErrorStrategy>();
  try {
    antlr4::ANTLRInputStream inputs(request_data);
    lexer._input = &inputs;
    lexer.reset();
    antlr4::CommonTokenStream tokens(&lexer);
    parser.setTokenStream(&tokens);
    parser.setBuildParseTree(false);
    parser.setErrorHandler(error_handler);
    parser.s();
    responses.emplace(parser.expression_value);
  } catch (const std::exception &err) {
    // TODO: make here throw with nested exception
    throw parse_error(err.what());
  }
}

void Responser::do_read() {
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
      std::copy(data_ptr, data_ptr + bytes_received, recv_buffer.data());
      DEBUG("incomplete header, bytes_received: {}", bytes_received);
      break;
    }
    // 此处有可能 bytes_received == 1，甚至都读不到实际的
    // header，这种情况怎么办？ read packet
    uint16_t data_size = get_content_size(data_ptr).size;
    size_t packet_size = data_size + header_size;
    if (packet_size > bytes_received) {
      std::copy(data_ptr, data_ptr + bytes_received, recv_buffer.data());
      // not fully read, wait for next read
      DEBUG("incomplete body");
      break;
    }
    std::string_view packet_data(data_ptr + header_size, data_size);
    do_response(packet_data);
    data_ptr += packet_size;
    bytes_received -= packet_size;
  }
  recv_buffer_bytes_written = bytes_received;
  recv_buffer_bytes_available = recv_buffer.size() - bytes_received;
}