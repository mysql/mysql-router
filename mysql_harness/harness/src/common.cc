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

#include "common.h"

#include <sstream>
#include <string.h>

namespace mysql_harness {

std::string get_strerror(int err) {
    char msg[256];
    std::string result;

#if  !_GNU_SOURCE && (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600)
  int ret = strerror_r(err, msg, sizeof(msg));
  if (ret) {
    return "errno= " + std::to_string(err) + " (strerror_r failed: " + std::to_string(ret) + ")";
  } else {
    result = std::string(msg);
  }
#elif defined(_WIN32)
  int ret = strerror_s(msg, sizeof(msg), err);
  if (ret) {
    return "errno= " + std::to_string(err) + " (strerror_s failed: " + std::to_string(ret) + ")";
  } else {
    result = std::string(msg);
  }
#else // GNU version
  char* ret = strerror_r(err, msg, sizeof(msg));
  result = std::string(ret);
#endif

  return result;
}

}
