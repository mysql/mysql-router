/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

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
#include "metadata.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <set>
#include <atomic>

#include "mysql/harness/logging/logging.h"

class ClusterMetadata;

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
                std::shared_ptr<MetaData> cluster_metadata,
                unsigned int ttl, const mysqlrouter::SSLOptions &ssl_options,
                const std::string &cluster_name);

  /** @brief Starts the Metadata Cache
   *
   * Starts the Metadata Cache and launch thread.
   */
  void start();

  /** @brief Stops the Metadata Cache
   *
   * Stops the Metadata Cache and the launch thread.
   */
  void stop() noexcept;

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
   * another. When an instance becomes unreachable, the rate of refresh of
   * the metadata cache increases to once per second until a (potentialy new) primary is detected.
   *
   * @param instance_id - the mysql_server_uuid that identifies the server instance
   * @param status - the status of the instance
   */
  void mark_instance_reachability(const std::string &instance_id,
                                  metadata_cache::InstanceStatus status);

  /** @brief Wait until there's a primary member in the replicaset
   *
   * To be called when the master of a single-master replicaset is down and
   * we want to wait until one becomes elected.
   *
   * @param replicaset_name name of the replicaset
   * @param timeout - amount of time to wait for a failover, in seconds
   * @return true if a primary member exists
   */
  bool wait_primary_failover(const std::string &replicaset_name, int timeout);
private:

  /** @brief Refreshes the cache
   *
   * Refreshes the cache.
   */
  void refresh();

  // Stores the list replicasets and their server instances.
  // Keyed by replicaset name
  std::map<std::string, metadata_cache::ManagedReplicaSet> replicaset_data_;

  // The name of the cluster in the topology.
  std::string cluster_name_;

  // The list of servers that contain the metadata about the managed
  // topology.
  std::vector<metadata_cache::ManagedInstance> metadata_servers_;

  // The time to live of the metadata cache.
  unsigned int ttl_;

  // SSL options for MySQL connections
  mysqlrouter::SSLOptions ssl_options_;

  // Stores the pointer to the transport layer implementation. The transport
  // layer communicates with the servers storing the metadata and fetches the
  // topology information.
  std::shared_ptr<MetaData> meta_data_;

  // Handle to the thread that refreshes the information in the metadata cache.
  std::thread refresh_thread_;

  // This mutex is used to ensure that a lookup of the metadata is consistent
  // with the changes in the metadata due to a cache refresh.
  std::mutex cache_refreshing_mutex_;

  #if 0 // not used so far
  // This mutex ensures that a refresh of the servers that contain the metadata
  // is consistent with the use of the server list.
  std::mutex metadata_servers_mutex_;
  #endif

  // Contains a set of replicaset names that have no primary
  std::set<std::string> lost_primary_replicasets_;

  std::mutex lost_primary_replicasets_mutex_;

  // Flag used to terminate the refresh thread.
  std::atomic_bool terminate_;

#ifdef FRIEND_TEST
  FRIEND_TEST(FailoverTest, basics);
  FRIEND_TEST(FailoverTest, primary_failover);
  FRIEND_TEST(MetadataCacheTest2, basic_test);
  FRIEND_TEST(MetadataCacheTest2, metadata_server_connection_failures);
#endif
};

#endif // METADATA_CACHE_METADATA_CACHE_INCLUDED
