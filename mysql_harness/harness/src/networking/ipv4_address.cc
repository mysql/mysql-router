/*
  Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "networking/ipv4_address.h"
#include "utilities.h"

#ifndef _WIN32
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#else
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif
#include <cerrno>
#include <cstring>
#include <string>

namespace mysql_harness {

IPv4Address::IPv4Address(const char *data) {
  if (inet_pton(AF_INET, data, &address_) <= 0) {
    throw std::invalid_argument(std::string("ipv4 parsing error"));
  }
}

std::string IPv4Address::str() const {
  char tmp[INET_ADDRSTRLEN];

  if (auto addr = inet_ntop(AF_INET, const_cast<in_addr*>(&address_),
                            tmp, INET_ADDRSTRLEN)) {
    return addr;
  }

  throw std::runtime_error(
      std::string("inet_ntop failed: ") + get_message_error(errno));
}

}
