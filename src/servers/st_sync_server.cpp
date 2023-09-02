#include "sync_calculator/responser.hpp"
#include "utils/server.hpp"

#include <cerrno>
#include <unistd.h>

#include <chrono>
#include <filesystem>

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
  } catch (const std::runtime_error &err) {
    fmt::print("{}\n\n", err.what());
    fmt::print("{}", parser);
  }

  try {
    std::string server_ip = parser.get<std::string>("--server-ip");
    uint16_t server_port = parser.get<uint16_t>("--server-port");
    server(server_ip, server_port, parser.get<int>("--backlog-size"));
  } catch (const std::exception &e) {
    ERROR(e.what());
    exit(-1);
  }
  return 0;
}