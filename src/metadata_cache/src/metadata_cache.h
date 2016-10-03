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

#ifndef METADATA_CACHE_METADATA_CACHE_INCLUDED
#define METADATA_CACHE_METADATA_CACHE_INCLUDED

#include "mysqlrouter/metadata_cache.h"
#include "metadata_factory.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <set>

#include "logger.h"


/** @class MetadataCache
 *
 * The MetadataCache manages cached information fetched from the
 * MySQL Server.
 *
 */
class METADATA_API MetadataCache {

public:
  /** @brief Constructor */
  MetadataCache(const std::vector<mysqlrouter::TCPAddress> &bootstrap_servers,
                const std::string &user, const std::string &password,
                int connection_timeout, int connection_attempts,
                unsigned int ttl, const std::string &cluster_name);

  /** @brief Destructor */
  ~MetadataCache();

  /** @brief Starts the Metadata Cache
   *
   * Starts the Metadata Cache and launch thread.
   */
  void start();

  /** @brief Stops the Metadata Cache
   *
   * Stops the Metadata Cache and the launch thread.
   */
  void stop();

  /** @brief Returns list of managed servers in a replicaset
   *
   * Returns list of managed servers in a replicaset.
   *
   * @param replicaset_name The ID of the replicaset being looked up
   * @return std::vector containing ManagedInstance objects
   */
  std::vector<metadata_cache::ManagedInstance> replicaset_lookup(
    const std::string &replicaset_name);

  /** @brief Update the status of the instance
   *
   * Called when an instance from a replicaset cannot be reached for one reason or
   * another. When a primary instance becomes unreachable, the rate of refresh of
   * the metadata cache increases to once per second until a new primary is detected.
   *
   * @param instance_id - the mysql_server_uuid that identifies the server instance
   * @param status - the status of the instance
   */
   void mark_instance_reachability(const std::string &instance_id,
                                   metadata_cache::InstanceStatus status);

private:

  /** @brief Refreshes the cache
   *
   * Refreshes the cache.
   */
  void refresh();

  // Stores the list of server instances in each replicaset. Given a
  // replicaset name, the map returns a list of ManagedInstances each of
  // which represent a list of servers in the replicaset.
  std::map<std::string, std::vector<metadata_cache::ManagedInstance>>
    replicaset_data_;

  // The name of the cluster in the topology.
  std::string cluster_name_;

  // The list of servers that contain the metadata about the managed
  // topology.
  std::vector<metadata_cache::ManagedInstance> metadata_servers_;

  // The time to live of the metadata cache.
  unsigned int ttl_;

  // Stores the pointer to the transport layer implementation. The transport
  // layer communicates with the servers storing the metadata and fetches the
  // topology information.
  std::shared_ptr<MetaData> meta_data_;

  // Handle to the thread that refreshes the information in the metadata cache.
  std::thread refresh_thread_;

  // This mutex is used to ensure that a lookup of the metadata is consistent
  // with the changes in the metadata due to a cache refresh.
  std::mutex cache_refreshing_mutex_;

  // This mutex ensures that a refresh of the servers that contain the metadata
  // is consistent with the use of the server list.
  std::mutex metadata_servers_mutex_;

  // Contains a set of replicaset names that have no primary
  std::set<std::string> lost_primary_replicasets_;

  std::mutex lost_primary_replicasets_mutex_;

  // Flag used to terminate the refresh thread.
  bool terminate_;
};

#endif // METADATA_CACHE_METADATA_CACHE_INCLUDED
