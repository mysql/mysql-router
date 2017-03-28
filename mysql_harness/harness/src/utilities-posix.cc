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

#include "utilities.h"

#include <fnmatch.h>
#include <unistd.h>

#include <string>

namespace mysql_harness {

namespace utility {

bool matches_glob(const std::string& word, const std::string& pattern) {
  return (fnmatch(pattern.c_str(), word.c_str(), 0) == 0);
}

void sleep_seconds(unsigned int seconds) {
  sleep(seconds);
}

}  // namespace utility

} // namespace mysql_harness
