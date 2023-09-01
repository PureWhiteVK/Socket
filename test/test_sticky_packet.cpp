#include "sync_calculator/requester.hpp"
#include "sync_calculator/responser.hpp"
#include "utils/client.hpp"
#include "utils/common.hpp"
#include "utils/server.hpp"

#include <unistd.h>

#include <chrono>
#include <filesystem>

void client(std::string_view server_ip, uint16_t server_port) {
  Client c{server_ip, server_port};
  c.connect();
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

  Requester req{c.handle()};

  int count = 5;
  try {
    while (count-- > 0) {
      // 一次发10个请求
      for (int i = 0; i < 10; i++) {
        req.do_request();
        req.do_write();
      }
      req.do_read();
    }
    // read all data until eof!
    while (req.has_requests()) {
      req.do_read();
    }
    int ret = shutdown(c.handle(), SHUT_RDWR);
    if (ret == -1) {
      THROW("failed to shutdown connection.\n{}", get_errno_string());
    }
  } catch (std::runtime_error &err) {
    INFO("error: {}", err.what());
  }
  close(c.handle());
}

void server(std::string_view server_ip, uint16_t server_port,
            int backlog_size) {
  Server s{server_ip, server_port};
  s.bind().listen(backlog_size);
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

  while (true) {
    try {
      Session sess = s.accept();
      // handle connection
      try {
        Responser resp{sess.handle()};
        while (true) {
          // read from client
          resp.do_read();
          resp.do_write();
        }
      } catch (std::runtime_error &err) {
        INFO("error: {}", err.what());
        close(sess.handle());
      }
    } catch (std::runtime_error &err) {
      INFO("error: {}", err.what());
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
  parser.add_argument("--backlog-size", "-b")
      .default_value<int>(1)
      .scan<'i', int>();

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
      server(server_ip, server_port, parser.get<int>("--backlog-size"));
    }
  } catch (std::exception &e) {
    fmt::println("{}", e.what());
    exit(-1);
  }
  return 0;
}