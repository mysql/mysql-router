/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "cmd_exec.h"
#include "router_test_helpers.h"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unistd.h>

using mysql_harness::Path;

Path get_cmake_source_dir() {
  char *env_value = std::getenv("CMAKE_SOURCE_DIR");
  Path result;
  if (env_value == nullptr) {
    // try a few places
    result = Path(get_cwd()).join("..");
    result = Path(realpath(result.c_str(), nullptr));
  } else {
    result = Path(realpath(env_value, nullptr));
  }

  if (!result.join("src").join("router").join("src").join("router_app.cc").is_regular()) {
    throw std::runtime_error(
        "Source directory not available. Use CMAKE_SOURCE_DIR environment variable; was " + result.str());
  }

  return result;
}

Path get_envvar_path(const std::string &envvar, Path alternative = Path()) {
  char *env_value = std::getenv(envvar.c_str());
  Path result;
  if (env_value == nullptr && alternative.is_directory()) {
    result = alternative;
  } else {
    result = Path(realpath(env_value, nullptr));
  }
  return result;
}

const std::string get_cwd() {
  char buffer[FILENAME_MAX];
  if (!getcwd(buffer, FILENAME_MAX)) {
    throw std::runtime_error("getcwd failed: " + std::string(strerror(errno)));
  }
  return std::string(buffer);
}

const std::string change_cwd(std::string &dir) {
  auto cwd = get_cwd();
  if (chdir(dir.c_str()) == -1) {
    throw std::runtime_error("chdir failed: " + std::string(strerror(errno)));
  }
  return cwd;
}

bool ends_with(const std::string &str, const std::string &suffix) {
  auto suffix_size = suffix.size();
  auto str_size = str.size();
  return (str_size >= suffix_size &&
          str.compare(str_size - suffix_size, str_size, suffix) == 0);
}

bool starts_with(const std::string &str, const std::string &prefix) {
  auto prefix_size = prefix.size();
  auto str_size = str.size();
  return (str_size >= prefix_size &&
          str.compare(0, prefix_size, prefix) == 0);
}

size_t read_bytes_with_timeout(int sockfd, void* buffer, size_t n_bytes, uint64_t timeout_in_ms) {

  // returns epoch time (aka unix time, etc), expressed in milliseconds
  auto get_epoch_in_ms = []()->uint64_t {
    using namespace std::chrono;
    time_point<system_clock> now = system_clock::now();
    return static_cast<uint64_t>(duration_cast<milliseconds>(now.time_since_epoch()).count());
  };

  // calculate deadline time
  uint64_t now_in_us = get_epoch_in_ms();
  uint64_t deadline_epoch_in_us = now_in_us + timeout_in_ms;

  // read until 1 of 3 things happen: enough bytes were read, we time out or read() fails
  size_t bytes_read = 0;
  while (true) {
    ssize_t res = read(sockfd, static_cast<char*>(buffer) + bytes_read, n_bytes - bytes_read);

    if (res == 0) {   // reached EOF?
      return bytes_read;
    }

    if (get_epoch_in_ms() > deadline_epoch_in_us)
    {
      throw std::runtime_error("read() timed out");
    }

    if (res == -1) {
      if (errno != EAGAIN) {
        throw std::runtime_error(std::string("read() failed: ") + strerror(errno));
      }
    } else {
      bytes_read += static_cast<size_t>(res);
      if (bytes_read >= n_bytes)
      {
        assert(bytes_read == n_bytes);
        return bytes_read;
      }
    }
  }
}

