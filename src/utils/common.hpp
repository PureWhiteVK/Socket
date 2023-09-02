#pragma once
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <sys/socket.h>

#include <chrono>
#include <exception>
#include <filesystem>

#include <argparse/argparse.hpp>
#include <spdlog/fmt/bundled/ostream.h>

#include "exceptions.hpp"
#include "spdlog/spdlog.h"

std::ostream &operator<<(std::ostream &os, const struct sockaddr_in &addr);

template <> struct fmt::formatter<struct sockaddr_in> : ostream_formatter {};
template <>
struct fmt::formatter<argparse::ArgumentParser> : ostream_formatter {};

#define SOCKADDR(x) reinterpret_cast<struct sockaddr *>(&(x))

#define THROW(...)                                                             \
  do {                                                                         \
    throw program_error(fmt::format("{}:{} {}", __FILE__, __LINE__,            \
                                    fmt::format(__VA_ARGS__)));                \
  } while (0)

#define TRACE(...)                                                             \
  do {                                                                         \
    spdlog::trace("{}:{} {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__));   \
  } while (0)

#define DEBUG(...)                                                             \
  do {                                                                         \
    spdlog::debug(__VA_ARGS__);                                                \
  } while (0)

#define INFO(...)                                                              \
  do {                                                                         \
    spdlog::info(__VA_ARGS__);                                                 \
  } while (0)

#define ERROR(...)                                                             \
  do {                                                                         \
    spdlog::error("{}:{} {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__));   \
  } while (0)

class scope_timer {
public:
  scope_timer(std::string_view name)
      : scope_name_{name}, enter_time_{std::chrono::steady_clock::now()} {}

  ~scope_timer() {
    std::chrono::steady_clock::time_point exit_time_{
        std::chrono::steady_clock::now()};
    spdlog::info("{} cost: {} ms", scope_name_,
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      exit_time_ - enter_time_)
                      .count());
  }

private:
  std::string_view scope_name_{};
  std::chrono::steady_clock::time_point enter_time_{};
};

#define TIMER_BEGIN(name)                                                      \
  {                                                                            \
    scope_timer __timer(name);

#define TIMER_END() }

#define CHECK(expr)                                                            \
  do {                                                                         \
    int ret = expr;                                                            \
    if (ret == -1) {                                                           \
      THROW("error executing: \n{}\n{}", #expr, get_errno_string(errno));      \
    }                                                                          \
  } while (false)

std::string to_human_readable(size_t size);

std::string get_errno_string(int _errno);

std::string get_fd_status_flag_string(int fd);

struct header {
  uint16_t size;
};

constexpr size_t header_size = sizeof(header);

inline void set_content_size(char *data, header data_size) {
  reinterpret_cast<header *>(data)->size = htons(data_size.size);
}
inline header get_content_size(char *data) {
  return {ntohs(reinterpret_cast<header *>(data)->size)};
}

inline int stoi(std::string_view v) {
  int res = 0;
  for (char ch : v) {
    res = res * 10 + (ch - '0');
  }
  return res;
}

inline void set_fd_status_flag(int fd, int flag) {
  int status_flag = fcntl(fd, F_GETFL);
  status_flag |= flag;
  CHECK(fcntl(fd, F_SETFL, status_flag));
}

inline void unset_fd_status_flag(int fd, int flag) {
  int status_flag = fcntl(fd, F_GETFL);
  status_flag &= ~flag;
  CHECK(fcntl(fd, F_SETFL, status_flag));
}

inline bool would_block(int _errno) {
  return _errno == EAGAIN || _errno == EWOULDBLOCK;
}

void set_addr(struct sockaddr_in &addr, std::string_view ip);

inline bool operator==(const struct sockaddr_in &a,
                       const struct sockaddr_in &b) {
  return a.sin_family == b.sin_family && a.sin_port == b.sin_port &&
         a.sin_addr.s_addr == b.sin_addr.s_addr;
}

std::string escaped(std::string_view data);

std::string octet_stream(std::string_view data);