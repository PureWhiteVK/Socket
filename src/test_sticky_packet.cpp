#include "util.hpp"

#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <queue>

#include "CalculatorLexer.h"
#include "CalculatorParser.h"
#include "antlr4-runtime.h"
#include <fmt/ranges.h>

void set_data_size(char *data, uint16_t data_size) {
  *reinterpret_cast<uint16_t *>(data) = htons(data_size);
}

uint16_t get_data_size(char *data) {
  return ntohs(*reinterpret_cast<uint16_t *>(data));
}

void client(std::string_view server_ip, uint16_t server_port) {
  Client c{server_ip, server_port};
  c.connect();
  int count = 5;
  // 如果设置了 linger ？
  {
    struct linger linger_opt {};
    linger_opt.l_linger = 0;
    linger_opt.l_onoff = 0;
    int ret = setsockopt(c.handle(), SOL_SOCKET, SO_LINGER, &linger_opt,
                         sizeof(linger_opt));
    if (ret == -1) {
      THROW("failed to set SO_LINGER option for socket.\n{}",
            get_errno_string());
    }
  }

  static constexpr size_t header_size = sizeof(uint16_t);

  std::queue<std::string> request_queue;

  std::function<bool()> do_write =
      [sock_fd = c.handle(), header_size = header_size, &request_queue]() {
        static std::array<char, 1024> send_buffer;
        // 数据的前两个字节表示本次请求的数据大小，后面的为实际请求数据（明文形式）
        std::string request_body =
            fmt::format("{}+{}", rand() % 100, rand() % 1000);
        request_queue.emplace(request_body);
        auto [_, data_size] = fmt::format_to_n(send_buffer.data() + header_size,
                                               send_buffer.size() - header_size,
                                               "{}", request_body);
        set_data_size(send_buffer.data(), data_size);
        size_t total_size = data_size + header_size;
        int ret = write(sock_fd, send_buffer.data(), total_size);
        if (ret == -1) {
          INFO("error: {}", get_errno_string());
          return false;
        }
        return true;
      };

  std::function<bool()> do_read = [sock_fd = c.handle(), &request_queue]() {
    static std::array<char, 1024> recv_buffer;
    static size_t capacity = recv_buffer.size();
    static size_t offset = 0;
    int bytes_received = read(sock_fd, recv_buffer.data() + offset, capacity);
    if (bytes_received == 0) {
      INFO("eof");
      return false;
    } else if (bytes_received == -1) {
      INFO("{}", get_errno_string());
      return false;
    }
    char *data = recv_buffer.data();
    int packet_count = 0;
    std::vector<std::string_view> responses;
    while (bytes_received > 0) {
      // read packet
      uint16_t data_size = get_data_size(data);
      size_t total_size = data_size + header_size;
      if (total_size > bytes_received) {
        // not fully read, wait for next read
        INFO("data not fully received, wait for next read");
        break;
      }
      bytes_received -= total_size;
      packet_count++;
      std::string_view packet_data(data + header_size, data_size);
      responses.emplace_back(packet_data);
      data += total_size;
    }
    INFO("responses in one read:", responses);
    for (auto &resp : responses) {
      fmt::println("{} = {}", request_queue.front(), resp);
      request_queue.pop();
    }
    offset = bytes_received;
    return true;
  };

  while (count-- > 0) {
    // 一次发10个请求
    for (int i = 0; i < 10; i++) {
      do_write();
    }
    do_read();
  }
  shutdown(c.handle(), SHUT_WR);
  // 等待服务端返回数据
  // 这里是一个细节
  while (do_read()) {
  }
  close(c.handle());
}

void server(std::string_view server_ip, uint16_t server_port) {
  Server s{server_ip, server_port};
  s.bind().listen(1);
  {
    int reuseaddr = 1;
    int ret = setsockopt(s.handle(), SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                         sizeof(reuseaddr));
    if (ret == -1) {
      THROW("failed to set SO_REUSEADDR option for server socket:\n{}",
            get_errno_string());
    }
  }
  {
    int reuseport = 1;
    int ret = setsockopt(s.handle(), SOL_SOCKET, SO_REUSEPORT, &reuseport,
                         sizeof(reuseport));
    if (ret == -1) {
      THROW("failed to set SO_REUSEPORT option for server socket:\n{}",
            get_errno_string());
    }
  }

  static constexpr size_t header_size = sizeof(uint16_t);
  std::queue<std::string> responses;

  std::function<bool(int, std::string_view)> do_write =
      [](int sock_fd, std::string_view response) -> bool {
    static std::array<char, 1024> send_buffer;
    auto [_, data_size] =
        fmt::format_to_n(send_buffer.data() + header_size,
                         send_buffer.size() - header_size, "{}", response);
    set_data_size(send_buffer.data(), data_size);
    int ret = write(sock_fd, send_buffer.data(), header_size + data_size);
    if (ret == -1) {
      INFO("{}", get_errno_string());
      return false;
    }
    return true;
  };

  std::function<int(std::string_view)> do_request =
      [](std::string_view expr) -> int {
    // run lexer + parser get calculation result
    antlr4::ANTLRInputStream inputs(expr);
    parser::CalculatorLexer lexer(&inputs);
    antlr4::CommonTokenStream tokens(&lexer);
    parser::CalculatorParser parser(&tokens);
    parser.setBuildParseTree(false);
    parser.s();
    // TODO: add error handler here
    return parser.expression_value;
  };

  std::function<bool(int)> do_read = [&responses,
                                      &do_request](int sock_fd) -> bool {
    static std::array<char, 1024> recv_buffer;
    static size_t offset = 0;
    static size_t capacity = recv_buffer.size();
    int bytes_received = read(sock_fd, recv_buffer.data() + offset, capacity);
    if (bytes_received == 0) {
      INFO("eof");
      return false;
    } else if (bytes_received == -1) {
      INFO("{}", get_errno_string());
      return false;
    }
    char *data = recv_buffer.data();
    int packet_count = 0;
    std::vector<std::string_view> requests;
    while (bytes_received > 0) {
      // read packet
      uint16_t data_size = get_data_size(data);
      size_t total_size = data_size + header_size;
      if (total_size > bytes_received) {
        // not fully read, wait for next read
        INFO("data not fully received, wait for next read");
        break;
      }
      bytes_received -= total_size;
      packet_count++;
      std::string_view packet_data(data + header_size, data_size);
      // INFO("request [{}]: {}", packet_count, packet_data);
      requests.emplace_back(packet_data);
      // 此处一定是按顺序处理的，因为 数据包是按顺序发送的
      responses.emplace(fmt::format("{}", do_request(packet_data)));
      data += total_size;
    }
    INFO("requests in one read:");
    for (auto req : requests) {
      fmt::println("{}", req);
    }
    offset = bytes_received;
    return true;
  };

  while (true) {
    Connection::Ref conn = s.accept();
    // handle connection
    {
      while (true) {
        // read from client
        if (!do_read(conn->handle())) {
          break;
        }
        while (!responses.empty()) {
          if (!do_write(conn->handle(), responses.front())) {
            break;
          }
          responses.pop();
        }
      }
      close(conn->handle());
    }
  }
}

namespace fs = std::filesystem;

int main(int argc, char **argv) {
  argparse::ArgumentParser parser(fs::path(argv[0]).filename());
  parser.add_argument("--client", "-c")
      .implicit_value(true)
      .default_value<bool>(false)
      .help("run as client");
  parser.add_argument("--server-ip", "-s")
      .default_value<std::string>("127.0.0.1");
  parser.add_argument("--server-port", "-p")
      .default_value<uint16_t>(7814)
      .scan<'i', uint16_t>();

  try {
    parser.parse_args(argc, argv);
  } catch (std::runtime_error &err) {
    fmt::println("{}\n", err.what());
    fmt::print("{}", parser);
  }

  try {
    std::string server_ip = parser.get<std::string>("--server-ip");
    uint16_t server_port = parser.get<uint16_t>("--server-port");
    if (parser.get<bool>("--client")) {
      client(server_ip, server_port);
    } else {
      server(server_ip, server_port);
    }
  } catch (std::exception &e) {
    fmt::println("{}", e.what());
    exit(-1);
  }
  return 0;
}