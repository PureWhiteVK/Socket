#include "util.hpp"

std::ostream &operator<<(std::ostream &os, const struct sockaddr_in &addr) {
  return os << fmt::format("{}:{}", inet_ntoa(addr.sin_addr),
                           ntohs(addr.sin_port));
}