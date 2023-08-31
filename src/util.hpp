#pragma once
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/socket.h>

#include <exception>

#include <argparse/argparse.hpp>
#include <fmt/core.h>
#include <fmt/ostream.h>

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

#ifdef TRACING
#define INFO(...)                                                              \
  do {                                                                         \
    fmt::println("{}:{} {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__));    \
  } while (0)
#else
#define INFO(...) true
#endif

class program_error : public std::runtime_error {
  using base = std::runtime_error;

public:
  template <typename... Args>
  program_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};

class eof_error : public std::runtime_error {
  using base = std::runtime_error;

public:
  template <typename... Args>
  eof_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};

class write_error : public std::runtime_error {
  using base = std::runtime_error;

public:
  template <typename... Args>
  write_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};

class read_error : public std::runtime_error {
  using base = std::runtime_error;

public:
  template <typename... Args>
  read_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};

std::string to_human_readable(size_t size);

std::string get_errno_string();

struct header_t {
  uint16_t size;
};

constexpr size_t header_size = sizeof(header_t);

inline void set_header(char *data, header_t data_size) {
  reinterpret_cast<header_t *>(data)->size = htons(data_size.size);
}
inline header_t get_header(char *data) {
  return {ntohs(reinterpret_cast<header_t *>(data)->size)};
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

class Session {
public:
  Session() = default;
  int handle() const { return m_fd; }
  const struct sockaddr_in &remote_endpoint() const {
    return m_remote_endpoint;
  }
  friend class Server;
  friend struct std::hash<Session>;
  friend bool operator==(const Session &a, const Session &b) {
    return a.m_fd == b.m_fd && a.m_remote_endpoint == b.m_remote_endpoint;
  }

private:
  int m_fd{};
  struct sockaddr_in m_remote_endpoint;
};

namespace std {
template <> struct hash<Session> {
  size_t operator()(const Session &s) const noexcept {
    return std::hash<int>()(s.m_fd);
  }
};
}; // namespace std

class Server {
public:
  Server(std::string_view ip, uint16_t port) {
    m_local_endpoint.sin_family = AF_INET;
    m_local_endpoint.sin_port = ::htons(port);
    set_addr(m_local_endpoint, ip);

    m_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_fd == -1) {
      THROW("failed to create server listen socket.\n{}", get_errno_string());
    }
  }

  Server(uint16_t port) {
    m_local_endpoint.sin_family = AF_INET;
    m_local_endpoint.sin_port = ::htons(port);
    m_local_endpoint.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    m_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_fd == -1) {
      THROW("failed to create server listen socket.\n{}", get_errno_string());
    }
  }

  Server &bind() {
    int ret =
        ::bind(m_fd, SOCKADDR(m_local_endpoint), sizeof(m_local_endpoint));
    if (ret == -1) {
      THROW("failed to bind {} to socket.\n{}", m_local_endpoint,
            get_errno_string());
    }
    return *this;
  }

  Server &listen(int backlog_size) {
    int ret = ::listen(m_fd, backlog_size);
    if (ret == -1) {
      THROW("failed to listen for connection on socket.\n{}",
            get_errno_string());
    }
    INFO("start server at: {}", m_local_endpoint);
    return *this;
  }

  int handle() const { return m_fd; }

  const struct sockaddr_in &local_endpoint() const { return m_local_endpoint; }

  Session accept() {
    Session sess = Session();
    socklen_t size = sizeof(sess.m_remote_endpoint);
    /*
        accept 的一个小坑！！！
        The addrlen argument is a value-result argument: the caller must
        initialize it to contain the size (in bytes) of the structure
        pointed to by addr; on return it will contain the actual size of
        the peer address.
    */
    sess.m_fd = ::accept(m_fd, SOCKADDR(sess.m_remote_endpoint), &size);
    if (sess.m_fd == -1) {
      THROW("failed to get connection on socket.\n{}", get_errno_string());
    }
    INFO("accept connection: {}", sess.m_remote_endpoint);
    return sess;
  }

private:
  int m_fd{};
  struct sockaddr_in m_local_endpoint {};
};

class Client {
public:
  Client(std::string_view remote_ip, uint16_t remote_port) {
    m_remote_endpoint.sin_family = AF_INET;
    m_remote_endpoint.sin_port = ::htons(remote_port);
    set_addr(m_remote_endpoint, remote_ip);

    m_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_fd == -1) {
      THROW("failed to create client connection socket.\n{}",
            get_errno_string());
    }
  }

  Client(uint16_t remote_port) {
    m_remote_endpoint.sin_family = AF_INET;
    m_remote_endpoint.sin_port = ::htons(remote_port);
    m_remote_endpoint.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);

    m_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_fd == -1) {
      THROW("failed to create client connection socket.\n{}",
            get_errno_string());
    }
  }

  int handle() const { return m_fd; }

  const struct sockaddr_in &remote_endpoint() const {
    return m_remote_endpoint;
  }

  Client &connect() {
    int ret =
        ::connect(m_fd, SOCKADDR(m_remote_endpoint), sizeof(m_remote_endpoint));
    if (ret == -1) {
      THROW("failed to connection to remote endpoint: {}\n{}",
            m_remote_endpoint, get_errno_string());
    }
    INFO("connect to remote endpoint: {}", m_remote_endpoint);
    return *this;
  }

private:
  int m_fd{};
  struct sockaddr_in m_remote_endpoint;
};
