#pragma once
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/socket.h>

#include <chrono>
#include <exception>
#include <filesystem>

#include <argparse/argparse.hpp>
#include <fmt/ostream.h>

#include "exceptions.hpp"

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

#define INFO(...)                                                              \
  do {                                                                         \
    fmt::println("{}:{} {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__));    \
  } while (0)

class scope_timer {
public:
  scope_timer(std::string_view name)
      : scope_name_{name}, enter_time_{std::chrono::steady_clock::now()} {}

  ~scope_timer() {
    std::chrono::steady_clock::time_point exit_time_{
        std::chrono::steady_clock::now()};
    fmt::println("[{}] run time: {:.3f}", scope_name_,
                 static_cast<float>(
                     std::chrono::duration_cast<std::chrono::microseconds>(
                         exit_time_ - enter_time_)
                         .count()) /
                     1000.0f);
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
      THROW("error executing: \n{}\n{}", #expr, get_errno_string());           \
    }                                                                          \
  } while (false)

std::string to_human_readable(size_t size);

std::string get_errno_string();

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

void set_addr(struct sockaddr_in &addr, std::string_view ip);

inline bool operator==(const struct sockaddr_in &a,
                       const struct sockaddr_in &b) {
  return a.sin_family == b.sin_family && a.sin_port == b.sin_port &&
         a.sin_addr.s_addr == b.sin_addr.s_addr;
}
