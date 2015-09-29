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

#include "dest_first_available.h"

int DestFirstAvailable::get_server_socket(int connect_timeout) noexcept {
  if (destinations_.empty()) {
    return -1;
  }

  // With only 1 destination, no need to lock and update the iterator
  if (destinations_.size() == 1) {
    return get_mysql_socket(destinations_.at(0), connect_timeout);
  }

  // We start the list at the currently available server
  for (size_t i = current_pos_; i < destinations_.size(); ++i) {
    auto sock = get_mysql_socket(destinations_.at(i), connect_timeout);
    if (sock != -1) {
      std::lock_guard<std::mutex> lock(mutex_update_);
      current_pos_ = i;
      return sock;
    }
  }

  // We are out of destinations. Next time we will try from the beginning of the list.
  std::lock_guard<std::mutex> lock(mutex_update_);
  current_pos_ = 0;
  return -1;
}
