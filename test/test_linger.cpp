#include "utils/client.hpp"
#include "utils/common.hpp"
#include "utils/server.hpp"

#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <vector>

#include <argparse/argparse.hpp>
#include <spdlog/fmt/bundled/core.h>

namespace fs = std::filesystem;
namespace ch = std::chrono;

std::string get_md5(const fs::path &file) {
  std::string command =
      fmt::format("md5sum -b {}", fs::absolute(file).string());
  INFO("exec: {}", command);
  FILE *command_output = popen(command.c_str(), "r");
  static std::array<char, 1024> buffer;
  std::stringstream s;
  while (!feof(command_output)) {
    size_t bytes_read = fread(buffer.data(), 1, buffer.size(), command_output);
    s.write(buffer.data(), bytes_read);
  }

  int status_code = pclose(command_output);
  if (status_code) {
    INFO("failed to execute command: {}, statuc code: {}", command,
         status_code);
    exit(status_code);
  }
  std::string res = s.str();
  // the size of md5 output is 32 hex
  // TODO: should we use std::regex to get result ?
  return res.substr(0, 32);
}

void client(std::string_view server_ip, uint16_t server_port, int linger_type,
            const fs::path &data_file) {
  Client c{server_ip, server_port};
  c.connect();

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
    INFO("enable SO_LINGER, l_onoff = {}, l_linger = {}", linger_opt.l_onoff,
         linger_opt.l_linger);
    CHECK(setsockopt(c.handle(), SOL_SOCKET, SO_LINGER, &linger_opt,
                     sizeof(linger_opt)));
  } else {
    INFO("disable SO_LINGER");
  }

  // 计算一下 md5 的输出
  INFO("md5 of {} is {}", data_file.filename().string(), get_md5(data_file));
  std::ifstream stream(data_file, std::ios::in | std::ios::binary);
  stream.seekg(0, std::fstream::end);
  size_t target_size = stream.tellg();
  stream.seekg(0);
  std::vector<char> data;
  data.resize(target_size);
  stream.read(data.data(), data.size());
  INFO("send {}, size {} to remote", data_file.filename().string(),
       to_human_readable(data.size()));
  write(c.handle(), data.data(), data.size());
  ch::high_resolution_clock::time_point begin =
      ch::high_resolution_clock::now();
  close(c.handle());
  ch::high_resolution_clock::time_point end = ch::high_resolution_clock::now();
  INFO("close() costs {:.3f} ms",
       ch::duration_cast<ch::microseconds>(end - begin).count() / 1000.0);
}

void server(std::string_view server_ip, uint16_t server_port) {
  Server s{server_ip, server_port};
  s.bind().listen(1);
  {
    int reuseaddr = 1;
    CHECK(setsockopt(s.handle(), SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                     sizeof(reuseaddr)));
  }
  {
    int reuseport = 1;
    CHECK(setsockopt(s.handle(), SOL_SOCKET, SO_REUSEPORT, &reuseport,
                     sizeof(reuseport)));
  }

  while (true) {
    try {
      Session sess = s.accept();
      // handle connection
      {
        static std::array<char, 32 * 1024> read_buffer;
        ssize_t read_size;
        size_t total_bytes = 0;
        fs::path output_path =
            fmt::format("output_{}.zip", static_cast<uint32_t>(sess.handle()));
        std::ofstream output_file(output_path,
                                  std::ios::out | std::ios::binary);
        while (true) {
          read_size =
              read(sess.handle(), read_buffer.data(), read_buffer.size());
          if (read_size == -1) {
            INFO("{}", get_errno_string(errno));
            break;
          } else if (read_size == 0) {
            INFO("eof");
            break;
          }
          output_file.write(read_buffer.data(), read_size);
          total_bytes += read_size;
        }
        INFO("recv: {} in total", to_human_readable(total_bytes));
        // 必须是接受缓冲和发送缓冲都为空的时候才行，否则就会 RST
        output_file.close();
        INFO("md5 of {} is {}", output_path.filename().string(),
             get_md5(output_path));
        close(sess.handle());
      }
    } catch (const std::runtime_error &err) {
      ERROR(err.what());
    }
  }
}

int main(int argc, char **argv) {
  argparse::ArgumentParser parser(fs::path(argv[0]).filename());
  parser.add_argument("--client", "-c")
      .default_value<bool>(false)
      .implicit_value(true)
      .help("run as client (default run as server)");
  parser.add_argument("--server-ip", "-s")
      .default_value<std::string>("127.0.0.1");
  parser.add_argument("--server-port", "-p")
      .default_value<uint16_t>(7814)
      .scan<'i', uint16_t>();
  parser.add_argument("--file", "-f")
      .help("file to upload to server")
      .metavar("PATH");
  parser.add_argument("--linger-type", "-l")
      .default_value<int>(-1)
      .help("linger option type (-1 means disable linger, value great or equal "
            "than 0 means enable linger, 0 means l_onoff = 0, 1 means l_onoff "
            "= 1, l_linger = 0, 2 means l_onoff = 2, l_linger > 0)")
      .scan<'i', int>()
      .metavar("TYPE(INT)");

  try {
    parser.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    fmt::print("{}\n\n", err.what());
    fmt::print("{}", parser);
  }

  try {
    if (parser.get<bool>("--client") && !parser.present("--file")) {
      fmt::print("argument \"--file\" is required when run as client!\n\n");
      fmt::print("{}", parser);
      exit(-1);
    }
    std::string server_ip = parser.get<std::string>("--server-ip");
    uint16_t server_port = parser.get<uint16_t>("--server-port");
    if (parser.get<bool>("--client")) {
      fs::path f = parser.get<std::string>("--file");
      if (!fs::is_regular_file(f)) {
        THROW("data file {} is invalid!", f.string());
      }
      client(server_ip, server_port, parser.get<int>("--linger-type"), f);
    } else {
      server(server_ip, server_port);
    }
  } catch (const std::exception &e) {
    ERROR(e.what());
    exit(-1);
  }
  return 0;
}