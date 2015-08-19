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

#include <iostream>
#include <sstream>
#include <sys/socket.h>

namespace mysqlrouter {

void TCPAddress::init_family() noexcept {

  std::ostringstream os;
  int result;

  ip_family_ = Family::UNKNOWN;

  if (addr.empty()) {
    return;
  }

  struct sockaddr_in6 saddr6;
  result = inet_pton(AF_INET6, addr.c_str(), &(saddr6.sin6_addr));
  if (result == 1) {
    ip_family_ = Family::IPV6;
  } else {
    struct sockaddr_in saddr4;
    result = inet_pton(AF_INET, addr.c_str(), &(saddr4.sin_addr));
    if (result == 1) {
      ip_family_ = Family::IPV4;
    }
  }
}

uint16_t TCPAddress::validate_port(uint32_t tcp_port) {
  if (tcp_port > UINT16_MAX) {
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

const char *TCPAddress::c_str() const {
  return str().c_str();
}

bool TCPAddress::is_valid() const {
  return !(addr.empty() || port == 0 || ip_family_ == Family::UNKNOWN);
}

} // namespace
