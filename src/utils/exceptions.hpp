#pragma once

#include <exception>

#include <fmt/ostream.h>

class program_error : public std::runtime_error {
  using base = std::runtime_error;

public:
  template <typename... Args>
  program_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};

class eof_error : public program_error {
  using base = program_error;

public:
  template <typename... Args>
  eof_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};

class send_error : program_error {
  using base = program_error;

public:
  template <typename... Args>
  send_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};

class recv_error : program_error {
  using base = program_error;

public:
  template <typename... Args>
  recv_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};