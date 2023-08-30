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

class program_error : public std::runtime_error {
  using base = std::runtime_error;

#define THROW(...)                                                             \
  do {                                                                         \
    throw program_error(fmt::format("{}:{} {}", __FILE__, __LINE__,            \
                                    fmt::format(__VA_ARGS__)));                \
  } while (0)

#define INFO(...)                                                              \
  do {                                                                         \
    fmt::println("{}:{} {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__));    \
  } while (0)

public:
  template <typename... Args>
  program_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};

std::string to_human_readable(size_t size);

std::string get_errno_string();

void set_addr(struct sockaddr_in &addr, std::string_view ip);

class Connection {
public:
  using Ref = std::shared_ptr<Connection>;
  int handle() const { return m_fd; }
  const struct sockaddr_in &remote_endpoint() const {
    return m_remote_endpoint;
  }
  static Ref create() { return Ref(new Connection); }
  friend class Server;

private:
  Connection() = default;

private:
  int m_fd{};
  struct sockaddr_in m_remote_endpoint;
};

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

  Connection::Ref accept() {
    Connection::Ref conn = Connection::create();
    socklen_t size = sizeof(conn->m_remote_endpoint);
    /*
        accept 的一个小坑！！！
        The addrlen argument is a value-result argument: the caller must
        initialize it to contain the size (in bytes) of the structure
        pointed to by addr; on return it will contain the actual size of
        the peer address.
    */
    conn->m_fd = ::accept(m_fd, SOCKADDR(conn->m_remote_endpoint), &size);
    if (conn->m_fd == -1) {
      THROW("failed to get connection on socket.\n{}", get_errno_string());
    }
    INFO("accept connection: {}", conn->m_remote_endpoint);
    return conn;
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
