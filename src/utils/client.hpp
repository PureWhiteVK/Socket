#pragma once
#include "common.hpp"
#include "exceptions.hpp"
#include "server.hpp"

class Client : public Session {
  using Base = Session;

public:
  Client() = default;

  Client(std::string_view remote_ip, uint16_t remote_port);

  Client(uint16_t remote_port);

  void connect();
  const struct sockaddr_in &local_endpoint();

  friend struct std::hash<Client>;

private:
  void create_socket();
};

namespace std {
template <> struct hash<Client> : public hash<Session> {};
}; // namespace std