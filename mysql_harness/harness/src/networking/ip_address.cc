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

#include "mysql/harness/networking/ip_address.h"
#include "mysql/harness/networking/ipv4_address.h"
#include "mysql/harness/networking/ipv6_address.h"

#include <algorithm>
#include <string>

namespace mysql_harness {

IPAddress::IPAddress(const std::string &data) {
  // IPv6 has at least 2 colons
  if (std::count(data.begin(), data.end(), ':') >= 2) {
    ipv6_address_ = IPv6Address(data); // throws std::invalid_argument
    address_type_ = AddressType::kIPv6;
  } else {
    ipv4_address_ = IPv4Address(data); // throws std::invalid_argument
    address_type_ = AddressType::kIPv4;
  }
}

std::string IPAddress::str() const {
  if (is_ipv4()) {
    return ipv4_address_.str();
  } else if (is_ipv6()) {
    return ipv6_address_.str();
  } else {
    return {};
  }
}

bool operator==(const IPAddress &a, const IPAddress &b) {
  if (a.address_type_ != b.address_type_) {
    return false;
  }

  if (a.address_type_ == IPAddress::AddressType::kIPv4) {
    return a.ipv4_address_ == b.ipv4_address_;
  } else {
    return a.ipv6_address_ == b.ipv6_address_;
  }
}

} // namespace mysql_harness
