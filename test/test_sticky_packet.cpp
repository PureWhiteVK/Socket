#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#include "sync_calculator/requester.hpp"
#include "sync_calculator/responser.hpp"
#include "utils/client.hpp"
#include "utils/common.hpp"
#include "utils/server.hpp"

#include <cerrno>
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
    CHECK(setsockopt(c.handle(), SOL_SOCKET, SO_LINGER, &linger_opt,
                     sizeof(linger_opt)));
  }

  Requester req{c.handle()};

  bool pass = true;

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
      THROW("failed to shutdown connection.\n{}", get_errno_string(errno));
    }
  } catch (const std::runtime_error &err) {
    ERROR("error: {}", err.what());
    pass = false;
  }
  close(c.handle());
  if (pass) {
    INFO("pass!");
  }
}

void server(std::string_view server_ip, uint16_t server_port,
            int backlog_size) {
  Server s{server_ip, server_port};
  s.bind().listen(backlog_size);
  {
    int reuseaddr = 1;
    CHECK(setsockopt(s.handle(), SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                     sizeof(reuseaddr)));
  }
  {
    int reuseport = 1;
    CHECK(setsockopt(s.handle(), SOL_SOCKET, SO_REUSEPORT, &reuseport,
                     sizeof(reuseport)));
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
      } catch (const std::runtime_error &err) {
        ERROR(err.what());
        close(sess.handle());
      }
    } catch (const std::runtime_error &err) {
      ERROR(err.what());
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

  spdlog::set_level(spdlog::level::debug);

  try {
    parser.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    fmt::print("{}\n\n", err.what());
    fmt::print("{}\n", parser);
  }

  try {
    std::string server_ip = parser.get<std::string>("--server-ip");
    uint16_t server_port = parser.get<uint16_t>("--server-port");
    if (parser.get<bool>("--client")) {
      client(server_ip, server_port);
    } else {
      server(server_ip, server_port, parser.get<int>("--backlog-size"));
    }
  } catch (const std::exception &e) {
    ERROR(e.what());
    exit(-1);
  }
  return 0;
}