/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include <cassert>
#include <cstdarg>
#include <algorithm>
#include <cstdlib>
#include <iostream>

#include "utils.h"

namespace mysqlrouter {

  vector<string> wrap_string(const string to_wrap, size_t width, size_t indent_size) {
    size_t curr_pos = 0;
    size_t wrap_pos = 0;
    size_t prev_pos = 0;
    string work{to_wrap};
    vector<string> res{};
    auto indent = string(indent_size, ' ');
    auto real_width = width - indent_size;

    size_t str_size = work.size();
    if (str_size < real_width) {
      res.push_back(indent + work);
    } else {
      work.erase(std::remove(work.begin(), work.end(), '\r'), work.end());
      std::replace(work.begin(), work.end(), '\t', ' '), work.end();
      str_size = work.size();

      do {
        curr_pos = prev_pos + real_width;

        // respect forcing newline
        wrap_pos = work.find("\n", prev_pos);
        if (wrap_pos == string::npos || wrap_pos > curr_pos) {
          // No new line found till real_width
          wrap_pos = work.find_last_of(" ", curr_pos);
        }
        if (wrap_pos != string::npos) {
          assert(wrap_pos - prev_pos != string::npos);
          res.push_back(indent + work.substr(prev_pos, wrap_pos - prev_pos));
          prev_pos = wrap_pos + 1;  // + 1 to skip space
        } else {
          break;
        }
      } while (str_size - prev_pos > real_width || work.find("\n", prev_pos) != string::npos);
      res.push_back(indent + work.substr(prev_pos));
    }

    return res;
  }

  void substitute_envvar(string &line) {
    size_t pos_start;
    size_t pos_end;

    pos_start = line.find("ENV{");
    if (pos_start == string::npos) {
      throw mysqlrouter::envvar_no_placeholder("No environment variable placeholder found");
    }

    pos_end = line.find("}", pos_start + 4);
    if (pos_end == string::npos) {
      throw mysqlrouter::envvar_bad_placeholder("Environment placeholder not closed");
    }

    auto env_var = line.substr(pos_start + 4, pos_end - pos_start - 4);
    if (env_var.empty()) {
      throw mysqlrouter::envvar_bad_placeholder("No environment variable name found in placeholder");
    }

    auto env_var_value = std::getenv(env_var.c_str());
    if (env_var_value == nullptr) {
      // Environment variable not set
      throw mysqlrouter::envvar_not_available(string{"Unknown environment variable " + env_var});
    }

    line.replace(pos_start, env_var.size() + 5, env_var_value);
  }

  string string_format(const char* format, ...) {

    va_list args;
    va_start(args, format);
    va_list args_next;
    va_copy(args_next, args);

    int size = std::vsnprintf(nullptr, 0, format, args);
    vector<char> buf(static_cast<size_t>(size) + 1U);
    va_end(args);

    std::vsnprintf(buf.data(), buf.size(), format, args_next);
    va_end(args_next);

    return string(buf.begin(), buf.end() - 1);
  }
}