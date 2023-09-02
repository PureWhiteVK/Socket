#pragma once

#include <exception>

#include <spdlog/fmt/bundled/ostream.h>

// 此处必须是 public 继承，否则无法进行 catch 捕获

class eof_error : public std::exception {};

class temporarily_unavailable_error : public std::exception {};

class program_error : public std::runtime_error {
  using base = std::runtime_error;

public:
  template <typename... Args>
  program_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};

class send_error : public program_error {
  using base = program_error;

public:
  template <typename... Args>
  send_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};

class recv_error : public program_error {
  using base = program_error;

public:
  template <typename... Args>
  recv_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};

class parse_error : public program_error {
  using base = program_error;

public:
  template <typename... Args>
  parse_error(fmt::format_string<Args...> fmt, Args &&...args)
      : base(fmt::format(fmt, args...)) {}
};
