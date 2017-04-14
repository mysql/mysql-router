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

#ifndef MYSQL_HARNESS_NETWORKING_RESOLVER_INCLUDED
#define MYSQL_HARNESS_NETWORKING_RESOLVER_INCLUDED

#include <string>
#include <vector>

#include "ip_address.h"
#include "harness_export.h"

namespace mysql_harness {

class HARNESS_EXPORT Resolver {
 public:
  /**
   * Resolves the hostname to one or more IP addresses
   *
   * @param name hostname to resolve
   * @return a `std::vector` containing instances of `IPAddress`
   */
  std::vector<IPAddress> hostname(const char *name) const;

  /** @overload */
  std::vector<IPAddress> hostname(const std::string &name) const {
    return hostname(name.c_str());
  }

  /**
   * Resolves a TCP service name to its TCP port
   *
   * This method will resolve a service name such as `http` to
   * it's standard port `80`.
   *
   * @throws std::invalid_argument when given service name could not
   * be resolved.
   * @param name service name to resolve
   * @return port number of the given service
   */
  uint16_t tcp_service_name(const char *name) const;

  uint16_t tcp_service_name(const std::string &name) const {
    return tcp_service_name(name.c_str());
  }

  /**
   * Resolves a TCP port to its service name
   *
   * This method will resolve a TCP port to its service name.
   * When no service name is associated with the port, the port
   * number is returned as string.
   *
   * @param port TCP port to resolve
   * @return service name or port as string
   */
  std::string tcp_service_port(uint16_t port) const;

 protected:
  typedef std::pair<uint16_t, std::string> ServiceCacheEntry;

  /** Lookup cached TCP port using service name
   *
   * @param name TCP service name
   * @return port number for service or 0 if not cached
   */
  uint16_t cached_tcp_service_by_name(const std::string &name) const;

  /** Lookup cached service name using TCP port
   *
   * @param port TCP port number
   * @return service name or empty string if not cached
   */
  std::string cached_tcp_service_by_port(uint16_t port) const;

  /** Cache holding resolved TCP services */
  mutable std::vector<ServiceCacheEntry> cache_tcp_services_{};
};


}

#endif // MYSQL_HARNESS_NETWORKING_RESOLVER_INCLUDED

