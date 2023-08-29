#include "util.hpp"
#include <argparse/argparse.hpp>
#include <assert.h>
#include <chrono>
#include <errno.h>
#include <filesystem>
#include <fmt/core.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
namespace ch = std::chrono;

uint16_t server_port = 9213;
std::string_view server_ip = "127.0.0.1";

std::string errno_to_string(int _errno) {
  // from errno and
  struct errno_info {
    std::string_view name;
    std::string_view description;
  };
  static std::unordered_map<int, errno_info> map{
      {1, {"EPERM", "Operation not permitted"}},
      {2, {"ENOENT", "No such file or directory"}},
      {3, {"ESRCH", "No such process"}},
      {4, {"EINTR", "Interrupted system call"}},
      {5, {"EIO", "Input/output error"}},
      {6, {"ENXIO", "No such device or address"}},
      {7, {"E2BIG", "Argument list too long"}},
      {8, {"ENOEXEC", "Exec format error"}},
      {9, {"EBADF", "Bad file descriptor"}},
      {10, {"ECHILD", "No child processes"}},
      {11, {"EAGAIN", "Resource temporarily unavailable"}},
      {12, {"ENOMEM", "Cannot allocate memory"}},
      {13, {"EACCES", "Permission denied"}},
      {14, {"EFAULT", "Bad address"}},
      {15, {"ENOTBLK", "Block device required"}},
      {16, {"EBUSY", "Device or resource busy"}},
      {17, {"EEXIST", "File exists"}},
      {18, {"EXDEV", "Invalid cross-device link"}},
      {19, {"ENODEV", "No such device"}},
      {20, {"ENOTDIR", "Not a directory"}},
      {21, {"EISDIR", "Is a directory"}},
      {22, {"EINVAL", "Invalid argument"}},
      {23, {"ENFILE", "Too many open files in system"}},
      {24, {"EMFILE", "Too many open files"}},
      {25, {"ENOTTY", "Inappropriate ioctl for device"}},
      {26, {"ETXTBSY", "Text file busy"}},
      {27, {"EFBIG", "File too large"}},
      {28, {"ENOSPC", "No space left on device"}},
      {29, {"ESPIPE", "Illegal seek"}},
      {30, {"EROFS", "Read-only file system"}},
      {31, {"EMLINK", "Too many links"}},
      {32, {"EPIPE", "Broken pipe"}},
      {33, {"EDOM", "Numerical argument out of domain"}},
      {34, {"ERANGE", "Numerical result out of range"}},
      {35, {"EDEADLK", "Resource deadlock avoided"}},
      {36, {"ENAMETOOLONG", "File name too long"}},
      {37, {"ENOLCK", "No locks available"}},
      {38, {"ENOSYS", "Function not implemented"}},
      {39, {"ENOTEMPTY", "Directory not empty"}},
      {40, {"ELOOP", "Too many levels of symbolic links"}},
      {11, {"EWOULDBLOCK", "Resource temporarily unavailable"}},
      {42, {"ENOMSG", "No message of desired type"}},
      {43, {"EIDRM", "Identifier removed"}},
      {44, {"ECHRNG", "Channel number out of range"}},
      {45, {"EL2NSYNC", "Level 2 not synchronized"}},
      {46, {"EL3HLT", "Level 3 halted"}},
      {47, {"EL3RST", "Level 3 reset"}},
      {48, {"ELNRNG", "Link number out of range"}},
      {49, {"EUNATCH", "Protocol driver not attached"}},
      {50, {"ENOCSI", "No CSI structure available"}},
      {51, {"EL2HLT", "Level 2 halted"}},
      {52, {"EBADE", "Invalid exchange"}},
      {53, {"EBADR", "Invalid request descriptor"}},
      {54, {"EXFULL", "Exchange full"}},
      {55, {"ENOANO", "No anode"}},
      {56, {"EBADRQC", "Invalid request code"}},
      {57, {"EBADSLT", "Invalid slot"}},
      {35, {"EDEADLOCK", "Resource deadlock avoided"}},
      {59, {"EBFONT", "Bad font file format"}},
      {60, {"ENOSTR", "Device not a stream"}},
      {61, {"ENODATA", "No data available"}},
      {62, {"ETIME", "Timer expired"}},
      {63, {"ENOSR", "Out of streams resources"}},
      {64, {"ENONET", "Machine is not on the network"}},
      {65, {"ENOPKG", "Package not installed"}},
      {66, {"EREMOTE", "Object is remote"}},
      {67, {"ENOLINK", "Link has been severed"}},
      {68, {"EADV", "Advertise error"}},
      {69, {"ESRMNT", "Srmount error"}},
      {70, {"ECOMM", "Communication error on send"}},
      {71, {"EPROTO", "Protocol error"}},
      {72, {"EMULTIHOP", "Multihop attempted"}},
      {73, {"EDOTDOT", "RFS specific error"}},
      {74, {"EBADMSG", "Bad message"}},
      {75, {"EOVERFLOW", "Value too large for defined data type"}},
      {76, {"ENOTUNIQ", "Name not unique on network"}},
      {77, {"EBADFD", "File descriptor in bad state"}},
      {78, {"EREMCHG", "Remote address changed"}},
      {79, {"ELIBACC", "Can not access a needed shared library"}},
      {80, {"ELIBBAD", "Accessing a corrupted shared library"}},
      {81, {"ELIBSCN", ".lib section in a.out corrupted"}},
      {82, {"ELIBMAX", "Attempting to link in too many shared libraries"}},
      {83, {"ELIBEXEC", "Cannot exec a shared library directly"}},
      {84, {"EILSEQ", "Invalid or incomplete multibyte or wide character"}},
      {85, {"ERESTART", "Interrupted system call should be restarted"}},
      {86, {"ESTRPIPE", "Streams pipe error"}},
      {87, {"EUSERS", "Too many users"}},
      {88, {"ENOTSOCK", "Socket operation on non-socket"}},
      {89, {"EDESTADDRREQ", "Destination address required"}},
      {90, {"EMSGSIZE", "Message too long"}},
      {91, {"EPROTOTYPE", "Protocol wrong type for socket"}},
      {92, {"ENOPROTOOPT", "Protocol not available"}},
      {93, {"EPROTONOSUPPORT", "Protocol not supported"}},
      {94, {"ESOCKTNOSUPPORT", "Socket type not supported"}},
      {95, {"EOPNOTSUPP", "Operation not supported"}},
      {96, {"EPFNOSUPPORT", "Protocol family not supported"}},
      {97, {"EAFNOSUPPORT", "Address family not supported by protocol"}},
      {98, {"EADDRINUSE", "Address already in use"}},
      {99, {"EADDRNOTAVAIL", "Cannot assign requested address"}},
      {100, {"ENETDOWN", "Network is down"}},
      {101, {"ENETUNREACH", "Network is unreachable"}},
      {102, {"ENETRESET", "Network dropped connection on reset"}},
      {103, {"ECONNABORTED", "Software caused connection abort"}},
      {104, {"ECONNRESET", "Connection reset by peer"}},
      {105, {"ENOBUFS", "No buffer space available"}},
      {106, {"EISCONN", "Transport endpoint is already connected"}},
      {107, {"ENOTCONN", "Transport endpoint is not connected"}},
      {108, {"ESHUTDOWN", "Cannot send after transport endpoint shutdown"}},
      {109, {"ETOOMANYREFS", "Too many references: cannot splice"}},
      {110, {"ETIMEDOUT", "Connection timed out"}},
      {111, {"ECONNREFUSED", "Connection refused"}},
      {112, {"EHOSTDOWN", "Host is down"}},
      {113, {"EHOSTUNREACH", "No route to host"}},
      {114, {"EALREADY", "Operation already in progress"}},
      {115, {"EINPROGRESS", "Operation now in progress"}},
      {116, {"ESTALE", "Stale file handle"}},
      {117, {"EUCLEAN", "Structure needs cleaning"}},
      {118, {"ENOTNAM", "Not a XENIX named type file"}},
      {119, {"ENAVAIL", "No XENIX semaphores available"}},
      {120, {"EISNAM", "Is a named type file"}},
      {121, {"EREMOTEIO", "Remote I/O error"}},
      {122, {"EDQUOT", "Disk quota exceeded"}},
      {123, {"ENOMEDIUM", "No medium found"}},
      {124, {"EMEDIUMTYPE", "Wrong medium type"}},
      {125, {"ECANCELED", "Operation canceled"}},
      {126, {"ENOKEY", "Required key not available"}},
      {127, {"EKEYEXPIRED", "Key has expired"}},
      {128, {"EKEYREVOKED", "Key has been revoked"}},
      {129, {"EKEYREJECTED", "Key was rejected by service"}},
      {130, {"EOWNERDEAD", "Owner died"}},
      {131, {"ENOTRECOVERABLE", "State not recoverable"}},
      {132, {"ERFKILL", "Operation not possible due to RF-kill"}},
      {133, {"EHWPOISON", "Memory page has hardware error"}},
      {95, {"ENOTSUP", "Operation not supported"}},
  };
  if (map.find(_errno) == map.end()) {
    return "unknown";
  }
  return fmt::format("{}({}): {}", map[_errno].name, _errno,
                     map[_errno].description);
}

std::string get_md5(const fs::path &file) {
  std::string command =
      fmt::format("md5sum -b {}", fs::absolute(file).string());
  fmt::println("exec: {}", command);
  FILE *command_output = popen(command.c_str(), "r");
  static std::array<char, 1024> buffer;
  std::stringstream s;
  while (!feof(command_output)) {
    size_t bytes_read = fread(buffer.data(), 1, buffer.size(), command_output);
    s.write(buffer.data(), bytes_read);
  }

  int status_code = pclose(command_output);
  if (status_code) {
    fmt::println("failed to execute command: {}, statuc code: {}", command,
                 status_code);
  }
  std::string res = s.str();
  // the size of md5 output is 32 hex
  // TODO: should we use std::regex to get result ?
  return res.substr(0, 32);
}

std::string bytes_to_human_read(size_t size) {
  static const size_t kb = 1L << 10;
  static const size_t mb = 1L << 20;
  static const size_t gb = 1L << 30;
  static const size_t tb = 1L << 40;
  int n_tb = size / tb;
  size -= n_tb * tb;
  int n_gb = size / gb;
  size -= n_gb * gb;
  int n_mb = size / mb;
  size -= n_mb * mb;
  int n_kb = size / kb;
  size -= n_kb * kb;
  return fmt::format("{} tb {} gb {} mb {} kb {} b", n_tb, n_gb, n_mb, n_kb,
                     size);
}

void client(int linger_type) {
  fmt::println("start client");
  int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (linger_type != -1) {
    struct linger linger_opt;
    memset(&linger_opt, 0, sizeof(linger_opt));
    if (linger_type == 0) {
      linger_opt.l_onoff = 0;
    } else if (linger_type == 1) {
      linger_opt.l_onoff = 1;
      linger_opt.l_linger = 0;
    } else if (linger_type == 2) {
      linger_opt.l_onoff = 1;
      linger_opt.l_linger = 1;
    }
    fmt::println("enable SO_LINGER, l_onoff = {}, l_linger = {}",
                 linger_opt.l_onoff, linger_opt.l_linger);
    setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));
  } else {
    fmt::println("disable SO_LINGER");
  }
  int reuse_addr = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(server_port);
  inet_pton(AF_INET, server_ip.data(), &addr.sin_addr);

  (void)connect(sockfd, SOCKADDR(addr), sizeof(addr));
  // md5 sum: 7c4eb41c0586b505041de313790d6d0a
  fs::path data_file = "/home/ubuntu/io-multiplexing/spec-github-data.zip";
  // 计算一下 md5 的输出
  fmt::println("md5 of {} is {}", data_file.filename().string(),
               get_md5(data_file));
  std::ifstream stream(data_file, std::ios::in | std::ios::binary);
  stream.seekg(0, std::fstream::end);
  size_t target_size = stream.tellg();
  stream.seekg(0);
  std::vector<char> data;
  data.resize(target_size);
  stream.read(data.data(), data.size());
  fmt::println("send {} to remote", bytes_to_human_read(data.size()));
  write(sockfd, data.data(), data.size());
  ch::high_resolution_clock::time_point begin =
      ch::high_resolution_clock::now();
  close(sockfd);
  ch::high_resolution_clock::time_point end = ch::high_resolution_clock::now();
  fmt::println("close() costs {:.3f} ms",
               ch::duration_cast<ch::microseconds>(end - begin).count() /
                   1000.0);
}

void server() {
  fmt::println("start server");
  int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(server_port);
  inet_pton(AF_INET, server_ip.data(), &addr.sin_addr);

  (void)bind(fd, SOCKADDR(addr), sizeof(addr));
  listen(fd, 1);

  while (true) {
    struct sockaddr_in remote_addr;
    socklen_t remote_addr_len;
    int sockfd = accept(fd, SOCKADDR(remote_addr), &remote_addr_len);
    fmt::println("[{:#x}] accept remote: {}", sockfd, remote_addr);
    static std::array<char, 32 * 1024> read_buffer;
    ssize_t read_size;
    size_t total_bytes = 0;
    fs::path output_path =
        fmt::format("/home/ubuntu/io-multiplexing/output_{}.zip",
                    static_cast<uint32_t>(sockfd));
    std::ofstream output_file(output_path, std::ios::out | std::ios::binary);
    while (true) {
      read_size = read(sockfd, read_buffer.data(), read_buffer.size());
      if (read_size == -1) {
        fmt::println("read error: {}", errno_to_string(errno));
        break;
      } else if (read_size == 0) {
        fmt::println("eof");
        break;
      }
      output_file.write(read_buffer.data(), read_size);
      //   fmt::println("recv: {} bytes", read_size);
      total_bytes += read_size;
    }
    fmt::println("recv: {} in total", bytes_to_human_read(total_bytes));
    output_file.close();
    fmt::println("md5 of {} is {}", output_path.filename().string(),
                 get_md5(output_path));

    close(sockfd);
  }
}

int main(int argc, char **argv) {

  argparse::ArgumentParser parser(fs::path(argv[0]).filename());
  parser.add_argument("--client", "-c")
      .default_value<bool>(false)
      .implicit_value(true)
      .help("run as client (default run as server)");
  parser.add_argument("--linger-type", "-l")
      .default_value<int>(-1)
      .help("linger option type (-1 means disable linger, value great or equal "
            "than 0 means enable linger, 0 means l_onoff = 0, 1 means l_onoff "
            "= 1, l_linger = 0, 2 means l_onoff = 2, l_linger > 0)")
      .scan<'i', int>()
      .metavar("TYPE(INT)");

  try {
    parser.parse_args(argc, argv);
  } catch (std::runtime_error &err) {
    fmt::print("{}\n\n", err.what());
    fmt::print("{}", parser);
  }

  if (parser.get<bool>("--client")) {
    client(parser.get<int>("--linger-type"));
  } else {
    server();
  }
  return 0;
}