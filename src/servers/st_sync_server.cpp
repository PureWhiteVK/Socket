#include "utils/server.hpp"
#include "sync_calculator/responser.hpp"

#include <unistd.h>

#include <chrono>
#include <filesystem>

void server(std::string_view server_ip, uint16_t server_port,
            int backlog_size) {
  Server s{server_ip, server_port};
  s.bind().listen(backlog_size);
  fmt::println("start server at: {}", s.local_endpoint());
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
        fmt::println("accept connection: {}", sess.remote_endpoint());
        while (true) {
          // read from client
          resp.do_read();
          resp.do_write();
        }
      } catch (std::runtime_error &err) {
        fmt::println("error: {}", err.what());
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
    server(server_ip, server_port, parser.get<int>("--backlog-size"));
  } catch (std::exception &e) {
    fmt::println("{}", e.what());
    exit(-1);
  }
  return 0;
}