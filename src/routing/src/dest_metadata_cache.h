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

#ifndef ROUTING_DEST_METADATA_CACHE_INCLUDED
#define ROUTING_DEST_METADATA_CACHE_INCLUDED

#include "destination.h"
#include "mysql_routing.h"
#include "mysqlrouter/uri.h"

#include <thread>

#include "mysqlrouter/datatypes.h"
#include "logger.h"

class DestMetadataCacheGroup final : public RouteDestination {
public:
   enum RoutingMode {
     ReadWrite,
     ReadOnly
   };

   /** @brief Constructor */
   DestMetadataCacheGroup(const std::string &metadata_cache,
                          const std::string &replicaset,
                          const std::string &mode,
                          const mysqlrouter::URIQuery &query,
                          const Protocol::Type protocol);

  /** @brief Copy constructor */
  DestMetadataCacheGroup(const DestMetadataCacheGroup &other) = delete;

  /** @brief Move constructor */
  DestMetadataCacheGroup(DestMetadataCacheGroup &&) = delete;

  /** @brief Copy assignment */
  DestMetadataCacheGroup &operator=(const DestMetadataCacheGroup &) = delete;

  /** @brief Move assignment */
  DestMetadataCacheGroup &operator=(DestMetadataCacheGroup &&) = delete;

  int get_server_socket(int connect_timeout, int *error) noexcept;

  void add(const std::string &, uint16_t) { }


  /** @brief Returns whether there are destination servers
   *
   * The empty() method always returns false for Metadata Cache.
   *
   * Checking whether the Metadata Cache is empty for given destination
   * might be to expensive. We leave this to the get_server() method.
   *
   * @return Always returns False for Metadata Cache destination.
   */
  bool empty() const noexcept {
    return false;
  }

  /** @brief Prepares destinations
   *
   * Prepares the list of destination by fetching data from the
   * Metadata Cache.
   */
  void prepare() noexcept {
    destinations_ = get_available(nullptr);
  }

  /** @brief The Metadata Cache to use
   *
   * cache_name is the the section key in the configuration of Metadata Cache.
   *
   * For example, given following Metadata Cache configuration, cache_name will be
   * set to "ham":
   *
   *     [metadata_cache.ham]
   *     host = metadata.example.com
   *
   */
  const std::string cache_name;

  /** @brief The HA Group which will be used for looking up managed servers */
  const std::string ha_replicaset;

  /** @brief Routing mode, usually set to read-only or read-write
   *
   * For example, given following Metadata Cache configuration:
   *
   *     [routing:metadata_read_only]
   *     ..
   *     destination = metadata-cache://ham/replicaset/homepage
   *
   * 'homepage' will be value of `ha_replicaset`.
   */
  RoutingMode routing_mode;

  /** @brief Query part of the URI given as destination in the configuration
   *
   * For example, given following Metadata Cache configuration:
   *
   *     [routing:metadata_read_only]
   *     ..
   *     destination = metadata_cache:///cluster_name/replicaset_name?allow_primary_reads=yes
   *
   * The 'allow_primary_reads' is part of uri_query.
   */
  const mysqlrouter::URIQuery uri_query;

private:
  /** @brief Initializes
   *
   * This method initialized the object. It goes of the URI query information
   * and sets members accordingly.
   */
  void init();

  /** @brief Gets available destinations from Metadata Cache
   *
   * This method gets the destinations using Metadata Cache information. It uses
   * the `metadata_cache::lookup_replicaset()` function to get a list of current managed
   * servers.
   *
   */
  std::vector<mysqlrouter::TCPAddress> get_available(std::vector<std::string> *server_ids);

  /** @brief Whether we allow a read operations going to the primary (master) */
  bool allow_primary_reads_;
  size_t current_pos_;
};


#endif // ROUTING_DEST_METADATA_CACHE_INCLUDED
