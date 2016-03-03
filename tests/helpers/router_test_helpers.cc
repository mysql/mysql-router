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

#include "router_test_helpers.h"
#include "cmd_exec.h"

#include <stdexcept>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

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
