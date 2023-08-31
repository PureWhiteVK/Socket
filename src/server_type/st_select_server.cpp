#include "requester.hpp"
#include "responser.hpp"
#include "util.hpp"

#include <sys/select.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <unordered_set>

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

  fd_set original_read_fds, original_write_fds, read_fds, write_fds;
  int max_fd_number = s.handle();

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
      memcpy(&read_fds, &original_read_fds, sizeof(original_read_fds));
      memcpy(&write_fds, &original_write_fds, sizeof(original_write_fds));
      // build fd_set for select
      int n_ready_fds =
          select(max_fd_number + 1, &read_fds, &write_fds, nullptr, nullptr);
      // The return value may be zero if the timeout expired
      // before any file descriptors became ready.
      if (n_ready_fds < 1) {
        THROW("select error: {}", get_errno_string());
      }
      // loop through all
      // check for new connections
      if (FD_ISSET(s.handle(), &read_fds)) {
        try {
          Session sess = s.accept();
          if (responsers.size() > (FD_SETSIZE - 1)) {
            INFO("reach FD_SETSIZE, abort");
            close(sess.handle());
          } else {
            responsers.emplace(sess, Responser(sess.handle()));
            FD_SET(sess.handle(), &original_read_fds);
            FD_SET(sess.handle(), &original_write_fds);
            max_fd_number = std::max(max_fd_number, sess.handle());
          }
        } catch (std::runtime_error &err) {
          INFO("error: {}", err.what());
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
        } catch (std::runtime_error &err) {
          INFO("error: {}", err.what());
          close(sess.handle());
          FD_CLR(sess.handle(), &original_read_fds);
          FD_CLR(sess.handle(), &original_write_fds);
          sess_iter = responsers.erase(sess_iter);
        }
      }

    } catch (std::runtime_error &err) {
      INFO("error: {}", err.what());
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