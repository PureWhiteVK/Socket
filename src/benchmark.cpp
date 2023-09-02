#include "sync_calculator/requester.hpp"
#include "utils/client.hpp"
#include "utils/common.hpp"
#include "utils/exceptions.hpp"

#include <condition_variable>
#include <exception>
#include <mutex>
#include <signal.h>
#include <sys/select.h>
#include <unistd.h>

#include <argparse/argparse.hpp>
#include <spdlog/fmt/bundled/std.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
#include <unordered_set>

namespace fs = std::filesystem;
namespace ch = std::chrono;

std::atomic<int> total_fail_connections = 0;
std::atomic<int> total_requests = 0;

std::condition_variable start_request;
std::condition_variable wait_connection;

std::atomic<bool> start = false;
std::atomic<bool> finish = false;
std::atomic<int> connected_count = 0;

void workload(std::string_view server_ip, uint16_t server_port, int n_clients) {
  INFO("[{}] n_clients: {}", std::this_thread::get_id(), n_clients);

  std::unordered_map<Client, Requester> requesters;
  requesters.reserve(n_clients);

  int n_fail_connections = 0;
  int n_total_requests = 0;

  fd_set original_read_fds, original_write_fds, read_fds, write_fds;
  int max_fd_number = -1;

  FD_ZERO(&original_read_fds);
  FD_ZERO(&original_write_fds);

  for (int i = 0; i < n_clients; i++) {
    try {
      auto client = Client{server_ip, server_port};
      client.connect();
      // 这个可以检测出来？？
      set_fd_status_flag(client.handle(), O_NONBLOCK);
      FD_SET(client.handle(), &original_read_fds);
      FD_SET(client.handle(), &original_write_fds);
      requesters.emplace(client, Requester{client.handle()});
    } catch (std::exception &err) {
      ERROR(err.what());
      n_fail_connections++;
    }
  }

  INFO("[{}] fail connections: {}", std::this_thread::get_id(),
       n_fail_connections);

  connected_count++;
  wait_connection.notify_all();

  std::mutex thread_mutex;
  std::unique_lock lock{thread_mutex};
  start_request.wait(lock, []() -> bool { return start; });

  INFO("[{}] start requesting", std::this_thread::get_id());

  try {
    while (!finish && !requesters.empty()) {
      max_fd_number =
          std::max_element(requesters.begin(), requesters.end(),
                           [](const std::pair<Client, Requester> &a,
                              const std::pair<Client, Requester> &&b) -> bool {
                             return a.first.handle() < b.first.handle();
                           })
              ->first.handle();
      memcpy(&read_fds, &original_read_fds, sizeof(original_read_fds));
      memcpy(&write_fds, &original_write_fds, sizeof(original_write_fds));
      int n_ready_fds;

      /*
        EBADF  An invalid file descriptor was given in one of the sets.
        (Perhaps a file descriptor that was already closed, or one
        on which an error has occurred.)  However, see BUGS.
      */

      CHECK(n_ready_fds = select(max_fd_number + 1, &read_fds, &write_fds,
                                 nullptr, nullptr));
      for (auto sess_iter = requesters.begin();
           !finish && sess_iter != requesters.end();) {
        auto &[sess, resq] = *sess_iter;
        try {
          if (FD_ISSET(sess.handle(), &write_fds)) {
            resq.do_request();
            resq.do_write();
            n_total_requests++;
          }
          if (FD_ISSET(sess.handle(), &read_fds)) {
            resq.do_read();
          }
          sess_iter++;
          std::this_thread::sleep_for(ch::milliseconds(1));
        } catch (const std::exception &err) {
          ERROR("error: {}", err.what());
          close(sess.handle());
          FD_CLR(sess.handle(), &original_read_fds);
          FD_CLR(sess.handle(), &original_write_fds);
          sess_iter = requesters.erase(sess_iter);
        }
      }
    }
  } catch (const std::exception &err) {
    ERROR(err.what());
    return;
  }

  INFO("[{}] total requests: {}", std::this_thread::get_id(), n_total_requests);
  INFO("[{}] read requests", std::this_thread::get_id());

  // 遍历一边，如果以及有完成的直接删除即可
  for (auto sess_iter = requesters.begin(); sess_iter != requesters.end();) {
    auto &[sess, resq] = *sess_iter;
    if (!resq.has_requests()) {
      shutdown(sess.handle(), SHUT_RDWR);
      close(sess.handle());
      FD_CLR(sess.handle(), &original_read_fds);
      FD_CLR(sess.handle(), &original_write_fds);
      sess_iter = requesters.erase(sess_iter);
    } else {
      sess_iter++;
    }
  }

  INFO("[{}] finish up", std::this_thread::get_id());

  try {
    while (!requesters.empty()) {
      max_fd_number =
          std::max_element(requesters.begin(), requesters.end(),
                           [](const std::pair<Client, Requester> &a,
                              const std::pair<Client, Requester> &&b) -> bool {
                             return a.first.handle() < b.first.handle();
                           })
              ->first.handle();
      memcpy(&read_fds, &original_read_fds, sizeof(original_read_fds));
      int n_ready_fds;
      CHECK(n_ready_fds = select(max_fd_number + 1, &read_fds, nullptr, nullptr,
                                 nullptr));
      for (auto sess_iter = requesters.begin();
           sess_iter != requesters.end();) {
        auto &[sess, resq] = *sess_iter;
        try {
          if (FD_ISSET(sess.handle(), &read_fds)) {
            // 这里 do_read 之后还需要检查一下是否已经读完了，否则就会一直阻塞在
            // select 上面
            resq.do_read();
          }
          if (!resq.has_requests()) {
            shutdown(sess.handle(), SHUT_RDWR);
            close(sess.handle());
            FD_CLR(sess.handle(), &original_read_fds);
            FD_CLR(sess.handle(), &original_write_fds);
            sess_iter = requesters.erase(sess_iter);
          } else {
            sess_iter++;
          }
        } catch (const std::exception &err) {
          ERROR(err.what());
          close(sess.handle());
          FD_CLR(sess.handle(), &original_read_fds);
          FD_CLR(sess.handle(), &original_write_fds);
          sess_iter = requesters.erase(sess_iter);
        }
      }
    }
  } catch (const std::exception &err) {
    ERROR(err.what());
    return;
  }

  total_requests += n_total_requests;
  total_fail_connections += n_fail_connections;

  INFO("[{}] all done", std::this_thread::get_id());
}

int main(int argc, char **argv) {

  argparse::ArgumentParser parser(fs::path(argv[0]).filename());
  parser.add_argument("--server-ip", "-s")
      .default_value<std::string>("127.0.0.1");
  parser.add_argument("--server-port", "-p")
      .default_value<uint16_t>(7814)
      .scan<'i', uint16_t>();
  parser.add_argument("--thread", "-n")
      .default_value<uint16_t>(1)
      .scan<'i', uint16_t>()
      .metavar("UINT");
  parser.add_argument("--time", "-t")
      .default_value<uint16_t>(5)
      .scan<'i', uint16_t>()
      .metavar("UINT");
  parser.add_argument("--client", "-c")
      .default_value<uint16_t>(1000)
      .scan<'i', uint16_t>()
      .metavar("UINT")
      .help("specify the number of clients");

  signal(SIGPIPE, SIG_IGN);

  try {
    parser.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    fmt::print("{}\n\n", err.what());
    fmt::print("{}", parser);
  }

  INFO("wait for connection establishment");

  uint16_t threads = parser.get<uint16_t>("--thread");
  std::vector<std::thread> workers;
  int total_clients = parser.get<uint16_t>("--client");
  int n_clients =
      static_cast<int>(ceil(total_clients / static_cast<float>(threads)));
  finish = false;
  for (auto i = 0; i < threads; i++) {
    workers.push_back(std::thread(workload,
                                  parser.get<std::string>("--server-ip"),
                                  parser.get<uint16_t>("--server-port"),
                                  std::min(total_clients, n_clients)));
    total_clients -= n_clients;
  }

  // 这里主线程需要等待所有的工作线程创建好连接
  std::mutex mutex;
  std::unique_lock lock{mutex};
  wait_connection.wait(
      lock, [threads]() -> bool { return connected_count == threads; });
  INFO("start benchmarking");
  start = true;
  start_request.notify_all();

  TIMER_BEGIN("main stopwatch")
  std::this_thread::sleep_for(ch::seconds(parser.get<uint16_t>("--time")));
  TIMER_END()
  finish = true;
  for (auto &th : workers) {
    th.join();
  }
  INFO("fail connections: {}", static_cast<int>(total_fail_connections));
  INFO("total requests: {}", static_cast<int>(total_requests));
}