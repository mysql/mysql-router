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

#include "mysqlrouter/datatypes.h"

#include <cstring>
#include <netdb.h>
#include <iostream>
#include <sstream>
#include <sys/socket.h>

namespace mysqlrouter {

void TCPAddress::detect_family() noexcept {
  // Function only run once by setting ip_family_ > Family::UNKNOWN
  ip_family_ = Family::INVALID;

  if (addr.empty()) {
    return;
  }

  struct addrinfo *servinfo, *info, hints;
  int err;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  err = getaddrinfo(addr.c_str(), nullptr, &hints, &servinfo);
  if (err != 0) {
    // We consider the IP/name to be invalid
    return;
  }

  // Get family and IP address
  for (info = servinfo; info != nullptr; info = info->ai_next) {
    if (info->ai_family == AF_INET6) {
      ip_family_ = Family::IPV6;
    } else if (info->ai_family == AF_INET) {
      ip_family_ = Family::IPV4;
    }
  }
  freeaddrinfo(servinfo);
}

uint16_t TCPAddress::validate_port(uint32_t tcp_port) {
  if (tcp_port < 1 || tcp_port > UINT16_MAX) {
    return 0;
  }
  return static_cast<uint16_t>(tcp_port);
}

string TCPAddress::str() const {
  std::ostringstream os;

  if (ip_family_ == Family::IPV6) {
    os << "[" << addr << "]";
  } else {
    os << addr;
  }

  if (port > 0) {
    os << ":" << port;
  }

  return os.str();
}

bool TCPAddress::is_valid() noexcept {
  if (ip_family_ == Family::UNKNOWN) {
    detect_family();
  }
  return !(addr.empty() || port == 0 || ip_family_ == Family::INVALID);
}

} // namespace
