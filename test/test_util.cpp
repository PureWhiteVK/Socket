#include "utils/common.hpp"

int main() {
  size_t a = 28786280;
  // size_t b = 2012;
  // size_t c = 12391233;
  // size_t d = 123912312333;
  // size_t f = 1231238792342123;
  fmt::print("{} bytes = {}\n", a, to_human_readable(a));
  // fmt::println("{} bytes = {}", b, to_human_readable(b));
  // fmt::println("{} bytes = {}", c, to_human_readable(c));
  // fmt::println("{} bytes = {}", d, to_human_readable(d));
  // fmt::println("{} bytes = {}", f, to_human_readable(f));

  fmt::print("{}\n", stoi("1468717"));

  try {
    struct sockaddr_in addr;
    set_addr(addr, "192.168.1.1");
  } catch (const program_error &e) {
    fmt::print("{}\n", e.what());
  }
  try {
    struct sockaddr_in addr {
      AF_INET
    };
    addr.sin_port = 1234;
    set_addr(addr, "::0");
  } catch (const program_error &e) {
    fmt::print("{}\n", e.what());
  }
  try {
    struct sockaddr_in addr {
      AF_INET
    };
    addr.sin_port = 1234;
    set_addr(addr, "192.168.1.1");
    fmt::print("addr: {}\n", addr);
  } catch (const program_error &e) {
    fmt::print("{}\n", e.what());
  }
  try {
    struct sockaddr_in addr {
      AF_INET6
    };
    addr.sin_port = 1234;
    set_addr(addr, "::0");
    fmt::print("addr: {}\n", addr);
  } catch (const program_error &e) {
    fmt::print("{}\n", e.what());
  }
  return 0;
}