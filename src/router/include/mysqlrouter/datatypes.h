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

#ifndef MYSQLROUTER_DATATYPES_INCLUDED
#define MYSQLROUTER_DATATYPES_INCLUDED

#include <arpa/inet.h>
#include <iostream>
#include <string>

using std::string;

namespace mysqlrouter {

/** @brief Defines an IP address with port number  */
class TCPAddress {
public:
  enum class Family {
    UNKNOWN = 0,
    IPV4 = 1,
    IPV6 = 2,
    INVALID = 9,
  };

  TCPAddress(string address = "", uint32_t tcp_port = 0)
      : addr(address), port(validate_port(tcp_port)), ip_family_(Family::UNKNOWN) {
    detect_family();
  }

  /** @brief Copy constructor */
  TCPAddress(const TCPAddress &other)
      : addr(other.addr), port(other.port), ip_family_(other.ip_family_) { }

  /** @brief Move constructor */
  TCPAddress(TCPAddress &&other)
      : addr(std::move(other.addr)), port(other.port), ip_family_(other.ip_family_) { }

  /** @brief Copy assignment */
  TCPAddress &operator=(const TCPAddress &other) {
    string *my_addr = const_cast<string *>(&this->addr);
    *my_addr = other.addr;
    uint16_t *my_port = const_cast<uint16_t *>(&this->port);
    *my_port = other.port;
    Family *my_family = const_cast<Family *>(&this->ip_family_);
    *my_family = other.ip_family_;
    return *this;
  }

  /** @brief Move assignment */
  TCPAddress &operator=(TCPAddress &&other) {
    string *my_addr = const_cast<string *>(&this->addr);
    *my_addr = other.addr;
    uint16_t *my_port = const_cast<uint16_t *>(&this->port);
    *my_port = other.port;
    Family *my_family = const_cast<Family *>(&this->ip_family_);
    *my_family = other.ip_family_;
    return *this;
  };

  /** @brief Returns the address as a string
   *
   * Returns the address as a string.
   *
   * @return instance of std::string
   */
  string str() const;

  /** @brief Compares two addresses for equality
   *
   */
  friend bool operator==(const TCPAddress &left, const TCPAddress &right) {
    return (left.addr == right.addr) && (left.port == right.port);
  }

  /** @brief Returns whether the TCPAddress is valid
   *
   * Returns whether the address and port are valid. This function also
   * detects the family when it was still Family::UNKNOWN.
   */
  bool is_valid() noexcept;

  /** @brief Returns whether the TCPAddress is IPv4
   *
   * Returns true when the address is IPv4; false
   * when it is IPv6.
   */
  bool is_ipv4();

  template<Family T>
  bool is_family() {
    if (ip_family_ == T) {
      return true;
    }
    return false;
  }

  /** @brief Network name IP */
  const string addr;
  /** @brief TCP port */
  const uint16_t port;

private:
  /** @brief Initialize the address family */
  void detect_family() noexcept;

  /** @brief Validates the given port number */
  uint16_t validate_port(uint32_t tcp_port);

  /** @brief Address family for this IP Address */
  Family ip_family_;
};

} // namespace mysqlrouter

#endif // MYSQLROUTER_UTILS_INCLUDED
