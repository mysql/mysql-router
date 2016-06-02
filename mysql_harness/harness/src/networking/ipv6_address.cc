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

#include "networking/ipv6_address.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>
#include <string>


namespace mysql_harness {

IPv6Address::IPv6Address(const char *data) {
  if (inet_pton(AF_INET6, data, &address_) <= 0) {
    throw std::invalid_argument(std::string("ipv6 parsing error"));
  }
}

std::string IPv6Address::str() const {
  char tmp[INET6_ADDRSTRLEN];

  if (inet_ntop(AF_INET6, &address_, tmp, INET6_ADDRSTRLEN)) {
    return tmp;
  }

  throw std::runtime_error(std::string("inet_ntop failed: ") + strerror(errno));
}

} // namespace mysql_harness