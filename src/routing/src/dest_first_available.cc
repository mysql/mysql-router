/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include "mysql/harness/logging/logging.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

IMPORT_LOG_FUNCTIONS()

int DestFirstAvailable::get_server_socket(int connect_timeout, int *error) noexcept {
  // Say for example, that we have three servers: A, B and C.
  // The active server should be failed-over in such fashion:
  //
  //   A -> B -> C -> no more connections (regardless of whether A and B go back up or not)
  //
  // This is what this function does.

  if (destinations_.empty()) {
    return -1;
  }

  // We start the list at the currently available server
  for (size_t i = current_pos_; i < destinations_.size(); ++i) {
    auto addr = destinations_.at(i);
    log_debug("Trying server %s (index %d)", addr.str().c_str(), i);
    auto sock = get_mysql_socket(addr, connect_timeout);
    if (sock >= 0) {
      current_pos_ = i;
      return sock;
    }
  }

  // We are out of destinations. Next time we will try from the beginning of the list.
#ifndef _WIN32
  *error = errno;
#else
  *error = WSAGetLastError();
#endif
  current_pos_ = destinations_.size();  // so for(..) above will no longer try to connect to a server
  return -1;
}
