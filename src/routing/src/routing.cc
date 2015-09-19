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

#include "mysqlrouter/routing.h"

namespace routing {

const int kDefaultWaitTimeout = 300;
const uint16_t kDefaultMaxConnections = 512;
const int kDefaultDestinationConnectionTimeout = 1;

const std::map<string, AccessMode> kAccessModeNames = {
    {"read-write", AccessMode::kReadWrite},
    {"read-only",  AccessMode::kReadOnly},
};

string get_access_mode_name(AccessMode access_mode) noexcept {
  for (auto &it: kAccessModeNames) {
    if (it.second == access_mode) {
      return it.first;
    }
  }
  return "";
}

} // routing