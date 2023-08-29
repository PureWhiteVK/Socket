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

#define SOCKADDR(x) reinterpret_cast<struct sockaddr *>(&x)