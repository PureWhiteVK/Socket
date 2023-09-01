#include "sync_calculator/requester.hpp"
#include "utils/client.hpp"
#include "utils/common.hpp"

#include <sys/select.h>
#include <unistd.h>

#include <argparse/argparse.hpp>
#include <fmt/std.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;
namespace ch = std::chrono;

std::atomic<bool> done = false;

std::atomic<int> total_fail_connections = 0;
std::atomic<int> total_requests = 0;

// 计算 qps

void workload(std::string_view server_ip, uint16_t server_port, int n_clients) {
  // 每个 client 都要持续向服务器发送数据，数据大小为多大？
  std::unordered_map<int, Requester> requesters;
  requesters.reserve(n_clients);
  int n_fail_connections = 0;
  int n_total_requests = 0;

  fd_set original_read_fds, original_write_fds, read_fds, write_fds;
  int max_fd_number = -1;

  FD_ZERO(&original_read_fds);
  FD_ZERO(&original_write_fds);
  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);

  // we want to wait for accept (kind of read)

  for (int i = 0; i < n_clients; i++) {
    auto client = Client{server_ip, server_port};
    try {
      client.connect();
      requesters.emplace(client.handle(), Requester(client.handle()));
      FD_SET(client.handle(), &original_read_fds);
      FD_SET(client.handle(), &original_write_fds);
    } catch (program_error &err) {
      n_fail_connections++;
    }
  }

  try {
    while (!done && !requesters.empty()) {
      max_fd_number =
          std::max_element(requesters.begin(), requesters.end(),
                           [](const std::pair<int, Requester> &a,
                              const std::pair<int, Requester> &&b) -> bool {
                             return a.first > b.first;
                           })
              ->first;
      memcpy(&read_fds, &original_read_fds, sizeof(original_read_fds));
      memcpy(&write_fds, &original_write_fds, sizeof(original_write_fds));
      // build fd_set for select
      int n_ready_fds;
      TIMER_BEGIN("select()")
      n_ready_fds =
          select(max_fd_number + 1, &read_fds, &write_fds, nullptr, nullptr);
      TIMER_END()
      // The return value may be zero if the timeout expired
      // before any file descriptors became ready.
      if (n_ready_fds < 1) {
        THROW("select error: {}", get_errno_string());
      }
      for (auto sess_iter = requesters.begin();
           sess_iter != requesters.end();) {
        auto &[sess, resp] = *sess_iter;
        try {
          if (FD_ISSET(sess, &write_fds)) {
            requesters[sess].do_request();
          }
          if (FD_ISSET(sess, &read_fds)) {
            requesters[sess].do_read();
            n_total_requests++;
          }
          sess_iter++;
        } catch (std::runtime_error &err) {
          INFO("error: {}", err.what());
          close(sess);
          FD_CLR(sess, &original_read_fds);
          FD_CLR(sess, &original_write_fds);
          sess_iter = requesters.erase(sess_iter);
        }
      }
    }
  } catch (std::runtime_error &err) {
    INFO("{}", err.what());
    return;
  }
  INFO("clean up!");
  try {
    while (!requesters.empty()) {
      max_fd_number =
          std::max_element(requesters.begin(), requesters.end(),
                           [](const std::pair<int, Requester> &a,
                              const std::pair<int, Requester> &&b) -> bool {
                             return a.first > b.first;
                           })
              ->first;
      memcpy(&read_fds, &original_read_fds, sizeof(original_read_fds));
      // build fd_set for select
      // why select will block here, means no state change ?
      // TIMER_BEGIN
      // int n_ready_fds =
      //     select(max_fd_number + 1, &read_fds, nullptr, nullptr, nullptr);
      // TIMER_END
      // The return value may be zero if the timeout expired
      // before any file descriptors became ready.
      // if (n_ready_fds < 1) {
      //   THROW("select error: {}", get_errno_string());
      // }
      for (auto sess_iter = requesters.begin();
           sess_iter != requesters.end();) {
        auto &[sess, resp] = *sess_iter;
        try {
          INFO("n requests: {}", requesters[sess].n_requests());
          if (requesters[sess].has_requests()) {
            // if (FD_ISSET(sess, &read_fds)) {
            requesters[sess].do_read();
            n_total_requests++;
            // }
            sess_iter++;
          } else {
            shutdown(sess, SHUT_RDWR);
            close(sess);
            FD_CLR(sess, &original_read_fds);
            FD_CLR(sess, &original_write_fds);
            sess_iter = requesters.erase(sess_iter);
          }
        } catch (std::runtime_error &err) {
          INFO("error: {}", err.what());
          close(sess);
          FD_CLR(sess, &original_read_fds);
          FD_CLR(sess, &original_write_fds);
          sess_iter = requesters.erase(sess_iter);
        }
      }
    }
  } catch (std::runtime_error &err) {
    INFO("{}", err.what());
    return;
  }

  total_requests += n_total_requests;
  total_fail_connections += n_fail_connections;
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

  try {
    parser.parse_args(argc, argv);
  } catch (std::runtime_error &err) {
    fmt::println("{}\n", err.what());
    fmt::print("{}", parser);
  }

  uint16_t threads = parser.get<uint16_t>("--thread");
  std::vector<std::thread> workers;
  int n_clients = static_cast<int>(
      ceil(parser.get<uint16_t>("--client") / static_cast<float>(threads)));
  done = false;
  for (auto i = 0; i < threads; i++) {
    workers.push_back(
        std::thread(workload, parser.get<std::string>("--server-ip"),
                    parser.get<uint16_t>("--server-port"), n_clients));
  }
  auto start = ch::steady_clock::now();
  std::this_thread::sleep_for(ch::seconds(parser.get<uint16_t>("--time")));
  auto end = ch::steady_clock::now();
  fmt::println("{:.3f}s passed",
               round(ch::duration_cast<ch::milliseconds>(end - start).count() /
                     1000.0f));
  done = true;
  for (auto &th : workers) {
    th.join();
  }
  fmt::println("fail connections: {}",
               static_cast<int>(total_fail_connections));
  fmt::println("total requests: {}", static_cast<int>(total_requests));
}