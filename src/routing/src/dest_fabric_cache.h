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

#ifndef ROUTING_DEST_FABRIC_CACHE_INCLUDED
#define ROUTING_DEST_FABRIC_CACHE_INCLUDED

#include "destination.h"
#include "mysql_routing.h"
#include "mysqlrouter/uri.h"

#include <thread>

#include "mysqlrouter/datatypes.h"
#include "logger.h"

using std::runtime_error;
using std::chrono::system_clock;
using mysqlrouter::TCPAddress;
using mysqlrouter::URIQuery;

const int kDefaultRefreshInterval = 3;

class DestFabricCacheGroup final : public RouteDestination {
public:
  /** @brief Constructor */
  DestFabricCacheGroup(const string fabric_cache, const string group, routing::AccessMode mode, URIQuery query) :
      cache_name(fabric_cache),
      ha_group(group),
      routing_mode(mode),
      uri_query(query),
      allow_primary_reads_(false),
      current_pos_(0),
      count_secondary_(0),
      count_primary_(0) {
    init();
  };

  /** @brief Copy constructor */
  DestFabricCacheGroup(const DestFabricCacheGroup &other) = delete;

  /** @brief Move constructor */
  DestFabricCacheGroup(DestFabricCacheGroup &&) = delete;

  /** @brief Copy assignment */
  DestFabricCacheGroup &operator=(const DestFabricCacheGroup &) = delete;

  /** @brief Move assignment */
  DestFabricCacheGroup &operator=(DestFabricCacheGroup &&) = delete;

  int get_server_socket(int connect_timeout, int *error) noexcept;

  void add(const string &, uint16_t) { }

  /** @brief Returns whether there are destination servers
   *
   * The empty() method always returns false for Fabric Cache.
   *
   * Checking whether the Fabric Cache is empty for given destination
   * might be to expensive. We leave this to the get_server() method.
   *
   * @return Always returns False for Fabric Cache destination.
   */
  bool empty() const noexcept {
    return false;
  }

  /** @brief Prepares destinations
   *
   * Prepares the list of destination by fetching data from the
   * Fabric Cache.
   */
  void prepare() noexcept {
    destinations_ = get_available();
  }

  /** @brief The Fabric Cache to use
   *
   * cache_name is the the section key in the configuration of Fabric Cache.
   *
   * For example, given following Fabric Cache configuration, cache_name will be
   * set to "ham":
   *
   *     [fabric_cache:ham]
   *     host = fabric.example.com
   *
   */
  const string cache_name;

  /** @brief The HA Group which will be used for looking up managed servers */
  const string ha_group;

  /** @brief Routing mode, usually set to read-only or read-write
   *
   * For example, given following Fabric Cache configuration:
   *
   *     [routing:fabric_read_only]
   *     ..
   *     destination = fabric-cache://ham/group/homepage
   *
   * 'homepage' will be value of `ha_group`.
   */
  const routing::AccessMode routing_mode;

  /** @brief Query part of the URI given as destination in the configuration
   *
   * For example, given following Fabric Cache configuration:
   *
   *     [routing:fabric_read_only]
   *     ..
   *     destination = fabric-cache://ham/group/homepage?allow_primary_reads=yes
   *
   * The 'allow_primary_reads' is part of uri_query.
   */
  const URIQuery uri_query;

private:
  /** @brief Initializes
   *
   * This method initialized the object. It goes of the URI query information
   * and sets members accordingly.
   */
  void init();

  /** @brief Gets available destinations from Fabric Cache
   *
   * This method gets the destinations using Fabric Cache information. It uses
   * the `fabric_cache::lookup_group()` function to get a list of current managed
   * servers.
   *
   */
  std::vector<TCPAddress> get_available();

  /** @brief Whether we allow a read operations going to the primary (master) */
  bool allow_primary_reads_;
  size_t current_pos_;
  int count_secondary_;
  int count_primary_;
};


#endif // ROUTING_DEST_FABRIC_CACHE_INCLUDED
