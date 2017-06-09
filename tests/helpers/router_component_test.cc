/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _WIN32
#  include <netdb.h>
#  include <netinet/in.h>
#  include <fcntl.h>
#  include <sys/un.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <unistd.h>
#else
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <direct.h>
#  include <stdio.h>
#endif

#include "router_component_test.h"

#include "process_launcher.h"
#include "mysqlrouter/utils.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <random>
#include <thread>

using mysql_harness::Path;

namespace {

template<typename Out>
void split_str(const std::string &input, Out result, char delim = ' ') {
  std::stringstream ss;
  ss.str(input);
  std::string item;
  while (std::getline(ss, item, delim)) {
    *(result++) = item;
  }
}

std::vector<std::string> split_str(const std::string &s, char delim = ' ') {
    std::vector<std::string> elems;
    split_str(s, std::back_inserter(elems), delim);
    return elems;
}

#ifndef _WIN32
int close_socket(int sock) {
  return close(sock);
}
#else
int close_socket(SOCKET sock) {
  return closesocket(sock);
}
#endif

}

RouterComponentTest::RouterComponentTest():
  data_dir_(COMPONENT_TEST_DATA_DIR) {
}

void RouterComponentTest::SetUp() {
  using mysql_harness::Path;;
  char *stage_dir_c = std::getenv("STAGE_DIR");
  stage_dir_ = Path(stage_dir_c ? stage_dir_c : "./stage");
#ifdef _WIN32
  if (!origin_dir_.str().empty()) {
    stage_dir_ = Path(stage_dir_.join(origin_dir_.basename()));
  }
  else {
    throw std::runtime_error("Origin dir not set");
  }
#endif

  plugin_dir_ = stage_dir_;
  plugin_dir_.append("lib");
#ifndef _WIN32
  plugin_dir_.append("mysqlrouter");
#endif

  auto get_exe_path = [&](const std::string &name) -> Path {
    Path path(stage_dir_);
    path.append("bin");
#ifdef _WIN32
    path.append(name + ".exe");
#else
    path.append(name);
#endif
    return path.real_path();
  };

  mysqlrouter_exec_ = get_exe_path("mysqlrouter");
  mysqlserver_mock_exec_ = get_exe_path("mysql_server_mock");
}

RouterComponentTest::CommandHandle
RouterComponentTest::launch_command(const std::string &command,
                                    const std::string &params,
                                    bool catch_stderr) {
  auto params_vec = split_str(params, ' ');
  const char* params_arr[MAX_PARAMS];
  get_params(command, params_vec, params_arr);

  return RouterComponentTest::CommandHandle(command, params_arr, catch_stderr);
}

RouterComponentTest::CommandHandle
RouterComponentTest::launch_router(const std::string &params,
                                   bool catch_stderr,
                                   bool with_sudo) {
  std::string sudo_str(with_sudo ? "sudo --non-interactive " : "");
  std::string cmd = sudo_str + mysqlrouter_exec_.str();

  return launch_command(cmd, params, catch_stderr);
}

RouterComponentTest::CommandHandle
RouterComponentTest::launch_mysql_server_mock(const std::string& json_file, unsigned port) {
  return launch_command(mysqlserver_mock_exec_.str(),
                        json_file + " " + std::to_string(port), true);
}

bool RouterComponentTest::wait_for_port_ready(unsigned port, unsigned timeout_msec,
                                              const std::string &hostname) {
  struct addrinfo hints, * ainfo;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(hostname.c_str(),
                           std::to_string(port).c_str(),
                           &hints, &ainfo);
  if (status != 0) {
    throw std::runtime_error(std::string("wait_for_port_ready(): getaddrinfo() failed: ")
                             + gai_strerror(status));
  }
  std::shared_ptr<void> exit_freeaddrinfo(nullptr, [&](void*){freeaddrinfo(ainfo);});

  const unsigned MSEC_STEP = 100;
  do {
    auto sock_id = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
    if (sock_id < 0) {
      throw std::runtime_error("wait_for_port_ready(): socket() failed: " + std::to_string(get_socket_errno()));
    }
    std::shared_ptr<void> exit_close_socket(nullptr, [&](void*){close_socket(sock_id);});

    status = connect(sock_id, ainfo->ai_addr, ainfo->ai_addrlen);
    if (status < 0) {
      unsigned step = std::min(timeout_msec, MSEC_STEP);
      std::this_thread::sleep_for(std::chrono::milliseconds(step));
      timeout_msec -= step;
    }
  } while(status < 0 && timeout_msec > 0);

  return status >= 0;
}

int RouterComponentTest::purge_dir(const std::string& dir) {
  return mysqlrouter::delete_recursive(dir);
}

std::string RouterComponentTest::get_tmp_dir(const std::string &name) {
#ifdef _WIN32
  char buf[MAX_PATH];
  auto res = GetTempPath(MAX_PATH, buf);
  if (res == 0 || res > MAX_PATH) {
    throw std::runtime_error("Could not get temporary directory");
  }

  auto generate_random_sequence = [](size_t len) -> std::string {
    std::random_device rd;
    std::string result;
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<unsigned long> dist(0, sizeof(alphabet) - 2);

    for (size_t i = 0; i < len; ++i) {
      result += alphabet[dist(rd)];
    }

    return result;
  };

  std::string dir_name = name + "-" + generate_random_sequence(10);
  std::string result = Path(buf).join(dir_name).str();
  int err = _mkdir(result.c_str());
  if (err != 0) {
    throw std::runtime_error("Error creating temporary directory " + result);
  }
  return result;
#else
  const size_t MAX_LEN = 256;
  const std::string pattern_str = std::string(name + "-XXXXXX");
  const char* pattern = pattern_str.c_str();
  if (strlen(pattern) >= MAX_LEN) {
    throw std::runtime_error("Could not create temporary directory, name too long");
  }
  char buf[MAX_LEN];
  strncpy(buf, pattern, sizeof(buf)-1);
  const char *res = mkdtemp(buf);
  if (res == nullptr) {
    throw std::runtime_error("Could not create temporary directory");
  }

  return std::string(res);
#endif
}

void RouterComponentTest::get_params(const std::string &command,
                                     const std::vector<std::string> &params_vec,
                                     const char* out_params[MAX_PARAMS]) {
  out_params[0] =  command.c_str();

  size_t i = 1;
  for (const auto& par: params_vec) {
    if (i >= MAX_PARAMS-1) {
      throw std::runtime_error("Too many parameters passed to the MySQLRouter");
    }
    out_params[i++] = par.c_str();
  }
  out_params[i] = nullptr;
}

bool RouterComponentTest::CommandHandle::expect_output(const std::string& str,
                                                       bool regex,
                                                       unsigned timeout_ms) {
  for (;;) {
    if (output_contains(str, regex)) return true;
    if (!read_output(timeout_ms)) return false;
  }
}


bool RouterComponentTest::CommandHandle::output_contains(const std::string& str,
                                                         bool regex) const {
  if (!regex) {
    return execute_output_raw_.find(str) != std::string::npos;
  }

  // regex
  return pattern_found(execute_output_raw_, str);
}

bool RouterComponentTest::CommandHandle::read_output(unsigned timeout_ms) {
  const size_t BUF_SIZE = 1024;
  char cmd_output[BUF_SIZE] = {0};
  bool result = launcher_.read(cmd_output, BUF_SIZE, timeout_ms) > 0;

  if (result) {
    handle_output(cmd_output);
    execute_output_raw_ += cmd_output;
  }
  return result;
}

void RouterComponentTest::CommandHandle::handle_output(const std::string &line) {
  for (auto &response: output_responses_) {
    const std::string &output = response.first;
    if (line.substr(0, output.size()) == output) {
      const char* resp = response.second.c_str();
      launcher_.write(resp, strlen(resp));
      return;
    }
  }
}

std::string RouterComponentTest::create_config_file(const std::string &content,
                                                    const std::string &directory,
                                                    const std::string &name) {
  Path file_path = Path(directory).join(name);
  std::ofstream ofs_config(file_path.str());

  if (!ofs_config.good()) {
    throw(std::runtime_error("Could not create config file " + file_path.str()));
  }

  ofs_config << "[DEFAULT]" << std::endl;
  ofs_config << "logging_folder =" << std::endl;
  ofs_config << "plugin_folder = " << plugin_dir_.str() << std::endl;
  ofs_config << "runtime_folder = " << stage_dir_.str() << std::endl;
  ofs_config << "config_folder = " << stage_dir_.str() << std::endl;
  ofs_config << "data_folder = " << stage_dir_.str() << std::endl << std::endl;

  ofs_config << content << std::endl;
  ofs_config.close();

  return file_path.str();
}
