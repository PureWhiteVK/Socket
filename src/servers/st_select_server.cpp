#include "sync_calculator/responser.hpp"
#include "utils/common.hpp"
#include "utils/exceptions.hpp"
#include "utils/server.hpp"

#include <exception>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <unordered_set>

void server(std::string_view server_ip, uint16_t server_port,
            int backlog_size) {
  Server s{server_ip, server_port};
  s.bind().listen(backlog_size);
  // set_fd_status_flag(s.handle(), O_NONBLOCK);
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

  fd_set original_read_fds, original_write_fds, read_fds, write_fds;
  int max_fd_number = -1;
  int prev_max_fd_number = max_fd_number;

  FD_ZERO(&original_read_fds);
  FD_ZERO(&original_write_fds);
  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);

  // we want to wait for accept (kind of read)
  FD_SET(s.handle(), &original_read_fds);

  // we have to wait on s.handle()、
  std::unordered_map<Session, Responser> responsers;

  while (true) {
    try {
      if (!responsers.empty()) {
        max_fd_number =
            std::max_element(
                responsers.begin(), responsers.end(),
                [](const std::pair<Session, Responser> &a,
                   const std::pair<Session, Responser> &&b) -> bool {
                  return a.first.handle() < b.first.handle();
                })
                ->first.handle();
      } else {
        max_fd_number = -1;
      }
      max_fd_number = std::max(max_fd_number, s.handle());
      if (prev_max_fd_number != max_fd_number) {
        prev_max_fd_number = max_fd_number;
      }
      memcpy(&read_fds, &original_read_fds, sizeof(original_read_fds));
      memcpy(&write_fds, &original_write_fds, sizeof(original_write_fds));
      // build fd_set for select
      int n_ready_fds;
      CHECK(n_ready_fds = select(max_fd_number + 1, &read_fds, &write_fds,
                                 nullptr, nullptr));
      // The return value may be zero if the timeout expired
      // before any file descriptors became ready.
      // loop through all
      // check for new connections
      if (FD_ISSET(s.handle(), &read_fds)) {
        try {
          Session sess = s.accept();
          if (responsers.size() < 1000) {
            responsers.emplace(sess, Responser(sess.handle()));
            FD_SET(sess.handle(), &original_read_fds);
            FD_SET(sess.handle(), &original_write_fds);
          } else {
            INFO("max connection reached, abort!");
            close(sess.handle());
          }
        } catch (const std::exception &err) {
          ERROR(err.what());
        }
      }

      // then we have to loop through all sessions
      // perform read & write
      for (auto sess_iter = responsers.begin();
           sess_iter != responsers.end();) {
        auto &[sess, resp] = *sess_iter;
        try {
          if (FD_ISSET(sess.handle(), &read_fds)) {
            responsers[sess].do_read();
          }
          if (FD_ISSET(sess.handle(), &write_fds)) {
            responsers[sess].do_write();
          }
          sess_iter++;
        } catch (const std::exception &err) {
          close(sess.handle());
          FD_CLR(sess.handle(), &original_read_fds);
          FD_CLR(sess.handle(), &original_write_fds);
          sess_iter = responsers.erase(sess_iter);
        }
      }

    } catch (const std::exception &err) {
      ERROR(err.what());
      exit(-1);
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

  signal(SIGPIPE, SIG_IGN);

  try {
    parser.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    fmt::print("{}\n\n", err.what());
    fmt::print("{}\n", parser);
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