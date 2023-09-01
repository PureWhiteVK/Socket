#include "common.hpp"
#include "exceptions.hpp"

class Client {
public:
  Client(std::string_view remote_ip, uint16_t remote_port);

  Client(uint16_t remote_port);

  int handle() const { return fd_; }

  const struct sockaddr_in &remote_endpoint() const { return remote_endpoint_; }

  const struct sockaddr_in &local_endpoint() const { return local_endpoint_; }

  Client &connect();

private:
  void create_socket();

private:
  int fd_{};
  struct sockaddr_in remote_endpoint_;
  struct sockaddr_in local_endpoint_;
};