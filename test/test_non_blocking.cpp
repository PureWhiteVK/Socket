#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#include "utils/common.hpp"

#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
// manipulate file descriptor
#include <fcntl.h>

#include <spdlog/fmt/std.h>

#include <exception>
#include <filesystem>
#include <sstream>
#include <unordered_map>

// namespace fs = std::filesystem;

/*
  On Linux, select() may report a socket file descriptor as "ready
  for reading", while nevertheless a subsequent read blocks.  This
  could for example happen when data has arrived but upon
  examination has the wrong checksum and is discarded.  There may
  be other circumstances in which a file descriptor is spuriously
  reported as ready.  Thus it may be safer to use O_NONBLOCK on
  sockets that should not block.
*/

/*
F_GETFL (void)
      Return (as the function result) the file access mode and
      the file status flags; arg is ignored.

F_SETFL (int)
      Set the file status flags to the value specified by arg.
      File access mode (O_RDONLY, O_WRONLY, O_RDWR) and file
      creation flags (i.e., O_CREAT, O_EXCL, O_NOCTTY, O_TRUNC)
      in arg are ignored.  On Linux, this command can change
      only the O_APPEND, O_ASYNC, O_DIRECT, O_NOATIME, and
      O_NONBLOCK flags.  It is not possible to change the
      O_DSYNC and O_SYNC flags; see BUGS, below.
*/

int main() {
  try {
    spdlog::set_level(spdlog::level::debug);
    // watch stdin
    int fd = 0;
    std::array<char, 1 << 16> buffer;
    // CHECK(fd = open(file.c_str(), O_RDONLY));
    // 一般的 read 就是这样
    ssize_t bytes_read = -1;
    // get fd status flags
    int fd_status_flags;
    CHECK(fd_status_flags = fcntl(fd, F_GETFL));
    INFO(get_fd_status_flag_string(fd));
    fd_status_flags |= O_NONBLOCK;
    CHECK(fcntl(fd, F_SETFL, fd_status_flags));
    CHECK(fd_status_flags = fcntl(fd, F_GETFL));
    INFO(get_fd_status_flag_string(fd));
    fd_set readfds;
    FD_ZERO(&readfds);

    while (true) {
      FD_SET(fd, &readfds);
      CHECK(select(fd + 1, &readfds, nullptr, nullptr, nullptr));
      // perform read
      TIMER_BEGIN("read()");
      bytes_read = read(fd, buffer.data(), buffer.size());
      TIMER_END();
      if (bytes_read == 0) {
        INFO("eof");
        break;
      } else if (bytes_read < 0) {
        int curr_errno = errno;
        if (curr_errno == EWOULDBLOCK || curr_errno == EAGAIN) {
          // INFO("unavailable yet, skip!");
          continue;
        }
        THROW(get_errno_string(curr_errno));
      }
      INFO("bytes_read: {}\n{}", bytes_read,
           escaped({buffer.data(), static_cast<size_t>(bytes_read)}));
    }

    // 我们再将其读取模式设置为不读取
    fd_status_flags = fcntl(fd, F_GETFL);
    INFO(get_fd_status_flag_string(fd));
    fd_status_flags &= ~O_NONBLOCK;
    CHECK(fcntl(fd, F_SETFL, fd_status_flags));
    INFO(get_fd_status_flag_string(fd));
    while (true) {
      TIMER_BEGIN("read()");
      bytes_read = read(fd, buffer.data(), buffer.size());
      TIMER_END();
      if (bytes_read == 0) {
        INFO("eof");
        break;
      } else if (bytes_read < 0) {
        THROW(get_errno_string(errno));
      }
      INFO("bytes_read: {}\n{}", bytes_read,
           escaped({buffer.data(), static_cast<size_t>(bytes_read)}));
    }
    CHECK(close(fd));
  } catch (const std::exception &e) {
    spdlog::error(e.what());
  }
  return 0;
}