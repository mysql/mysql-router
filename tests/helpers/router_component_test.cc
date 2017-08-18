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
#  include <sys/un.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <sys/file.h>
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
#  include <io.h>
#  include <locale>
#  include <codecvt>
#endif

#include <fcntl.h>
#include "router_component_test.h"

#include "process_launcher.h"
#include "mysqlrouter/utils.h"

#include <algorithm>
#include <chrono>
#include <iterator>
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
                                    bool catch_stderr) const {
  auto params_vec = split_str(params, ' ');
  const char* params_arr[MAX_PARAMS];
  get_params(command, params_vec, params_arr);

  return RouterComponentTest::CommandHandle(command, params_arr, catch_stderr);
}

RouterComponentTest::CommandHandle
RouterComponentTest::launch_router(const std::string &params,
                                   bool catch_stderr,
                                   bool with_sudo) const {
  std::string sudo_str(with_sudo ? "sudo --non-interactive " : "");
  std::string cmd = sudo_str + mysqlrouter_exec_.str();

  return launch_command(cmd, params, catch_stderr);
}

RouterComponentTest::CommandHandle
RouterComponentTest::launch_mysql_server_mock(const std::string& json_file, unsigned port) const {
  return launch_command(mysqlserver_mock_exec_.str(),
                        json_file + " " + std::to_string(port), true);
}

bool RouterComponentTest::wait_for_port_ready(unsigned port, unsigned timeout_msec,
                                              const std::string &hostname) const {
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

/*static*/
int RouterComponentTest::purge_dir(const std::string& dir) {
  return mysql_harness::delete_dir_recursive(dir);
}

/*static*/
std::string RouterComponentTest::get_tmp_dir(const std::string &name) {
  return mysql_harness::get_tmp_dir(name);
}

void RouterComponentTest::get_params(const std::string &command,
                                     const std::vector<std::string> &params_vec,
                                     const char* out_params[MAX_PARAMS]) const {
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

int RouterComponentTest::CommandHandle::wait_for_exit(unsigned timeout_ms) {

  while (read_output(200)) {}

  timeout_ms -= std::min(200u, timeout_ms);

  exit_code_ = launcher_.wait(timeout_ms);
  exit_code_set_ = true;

  return exit_code_;
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
  bool result = launcher_.read(cmd_output, BUF_SIZE-1, timeout_ms) > 0;

  if (result) {
    handle_output(cmd_output);
    execute_output_raw_ += cmd_output;
  }
  return result;
}

void RouterComponentTest::CommandHandle::handle_output(const std::string &line) {
  for (const auto &response: output_responses_) {
    const std::string &output = response.first;
    if (line.substr(0, output.size()) == output) {
      const char* resp = response.second.c_str();
      launcher_.write(resp, strlen(resp));
      return;
    }
  }
}

std::map<std::string, std::string> RouterComponentTest::get_DEFAULT_defaults() const {
  return {
    {"logging_folder", ""},
    {"plugin_folder", plugin_dir_.str()},
    {"runtime_folder", stage_dir_.str()},
    {"config_folder", stage_dir_.str()},
    {"data_folder", stage_dir_.str()},
  };
}

std::string RouterComponentTest::make_DEFAULT_section(const std::map<std::string, std::string>* params) const {
  auto l = [params](const char* key) -> std::string {
    return (params->count(key))
        ? std::string(key) + " = " + params->at(key) + "\n"
        : "";
  };

  return params
    ? std::string("[DEFAULT]\n")
        + l("logging_folder")
        + l("plugin_folder")
        + l("runtime_folder")
        + l("config_folder")
        + l("data_folder")
        + "\n"
    : std::string("[DEFAULT]\n")
        + "logging_folder =\n"
        + "plugin_folder = "  + plugin_dir_.str() + "\n"
        + "runtime_folder = " + stage_dir_.str() + "\n"
        + "config_folder = "  + stage_dir_.str() + "\n"
        + "data_folder = "    + stage_dir_.str() + "\n\n";
}

std::string RouterComponentTest::create_config_file(const std::string &content,
                                                    const std::map<std::string, std::string> *params,
                                                    const std::string &directory,
                                                    const std::string &name) const {
  Path file_path = Path(directory).join(name);
  std::ofstream ofs_config(file_path.str());

  if (!ofs_config.good()) {
    throw(std::runtime_error("Could not create config file " + file_path.str()));
  }

  ofs_config << make_DEFAULT_section(params);
  ofs_config << content << std::endl;
  ofs_config.close();

  return file_path.str();
}


#ifndef _WIN32
bool UniqueId::lock_file(const std::string& file_name) {
  lock_file_fd_ = open(file_name.c_str(), O_RDWR | O_CREAT, 0666);

  if (lock_file_fd_ >= 0) {
#ifdef __sun
    struct flock fl;

    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;

    int lock = fcntl(lock_file_fd_, F_SETLK, &fl);
#else
    int lock = flock(lock_file_fd_, LOCK_EX | LOCK_NB);
#endif
    if (lock) {
      // no lock so no luck, try the next one
      close(lock_file_fd_);
      return false;
    }

    // obtained the lock
    return true;
  }

  return false;
}

std::string UniqueId::get_lock_file_dir() const {
  // this is what MTR uses, see mysql-test/lib/mtr_unique.pm for details
  return "/tmp/mysql-unique-ids";
}

#else

bool UniqueId::lock_file(const std::string& file_name) {
  lock_file_fd_ = ::CreateFile(file_name.c_str(), GENERIC_READ, 0, NULL, OPEN_ALWAYS, 0, NULL);
  if (lock_file_fd_ != NULL && lock_file_fd_ != INVALID_HANDLE_VALUE) {
    return true;
  }

  return false;
}

std::string UniqueId::get_lock_file_dir() const {
  // this are env variables that MTR uses, see mysql-test/lib/mtr_unique.pm for details
  DWORD buff_size = 65535;
  std::vector<char> buffer;
  buffer.resize(buff_size);
  buff_size = GetEnvironmentVariableA("ALLUSERSPROFILE", &buffer[0], buff_size);
  if (!buff_size) {
    buff_size = GetEnvironmentVariableA("TEMP", &buffer[0], buff_size);
  }

  if (!buff_size) {
    throw std::runtime_error("Could not get directory for lock files.");
  }

  std::string result(buffer.begin(), buffer.begin()+buff_size);
  result.append("\\mysql-unique-ids");
  return result;
}

#endif

UniqueId::UniqueId(unsigned start_from, unsigned range) {
  const std::string lock_file_dir = get_lock_file_dir();
  mysqlrouter::mkdir(lock_file_dir, 0777);

  for (unsigned i = 0; i < range; i++) {
    id_ = start_from + i;
    Path lock_file_path(lock_file_dir);
    lock_file_path.append(std::to_string(id_));
    lock_file_name_ = lock_file_path.str();

    if (lock_file(lock_file_name_.c_str())) {
      // obtained the lock, we are good to go
      return;
    }
  }

  throw std::runtime_error("Could not get uniqe id from the given range");
}

UniqueId::~UniqueId() {
#ifndef _WIN32
  if (lock_file_fd_ > 0) {
    close(lock_file_fd_);
  }
#else
  if (lock_file_fd_ != NULL && lock_file_fd_ != INVALID_HANDLE_VALUE) {
    ::CloseHandle(lock_file_fd_);
  }
#endif

  if (!lock_file_name_.empty()) {
    mysql_harness::delete_file(lock_file_name_);
  }
}

UniqueId::UniqueId(UniqueId&& other) {
  id_ = other.id_;
  lock_file_fd_ = other.lock_file_fd_;
  lock_file_name_ = other.lock_file_name_;

  // mark moved object as no longer owning of the resources
#ifndef _WIN32
  other.lock_file_fd_ = -1;
#else
  other.lock_file_fd_ = INVALID_HANDLE_VALUE;
#endif

  other.lock_file_name_ = "";
}

unsigned TcpPortPool::get_next_available() {
  if (number_of_ids_used_ >= kMaxPort) {
    throw std::runtime_error("No more available ports from UniquePortsGroup");
  }

  // this is the formula that mysql-test also uses to map lock filename to actual port number
  return 10000 + unique_id_.get() * kMaxPort + number_of_ids_used_++;
}
