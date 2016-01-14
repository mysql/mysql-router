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

#include "dest_first_available.h"

int DestFirstAvailable::get_server_socket(int connect_timeout, int *error) noexcept {
  if (destinations_.empty()) {
    return -1;
  }

  // We start the list at the currently available server
  for (size_t i = current_pos_; i < destinations_.size(); ++i) {
    auto addr = destinations_.at(i);
    log_debug("Trying server %s (index %d)", addr.str().c_str(), i);
    auto sock = get_mysql_socket(addr, connect_timeout);
    if (sock != -1) {
      current_pos_ = i;
      return sock;
    }
  }

  // We are out of destinations. Next time we will try from the beginning of the list.
  *error = errno;
  current_pos_ = 0;
  return -1;
}
