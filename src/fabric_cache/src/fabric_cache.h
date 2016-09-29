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

#ifndef FABRIC_CACHE_FABRIC_CACHE_INCLUDED
#define FABRIC_CACHE_FABRIC_CACHE_INCLUDED

#include "mysqlrouter/fabric_cache.h"
#include "fabric_factory.h"
#include "logger.h"
#include "utils.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>

#include "mysql/harness/logger.h"

using fabric_cache::ManagedServer;
using fabric_cache::ManagedShard;

const int kDefaultTimeToLive = 10;

/** @class FabricCache
 *
 * The FabricCache manages cached information fetched from the
 * MySQL Server.
 *
 */
class FabricCache {

public:
  /** @brief Constructor */
  FabricCache(std::string host, int port, std::string user, std::string password,
              int connection_timeout, int connection_attempts);

  /** @brief Destructor */
  ~FabricCache();

  /** Starts the Fabric Cache
   *
   * Starts the Fabric Cache and launch thread.
   */
  void start();

  /** @brief Returns list of managed servers in a group
   *
   * Returns list of managed servers in a group.
   *
   * @param group_id The ID of the group being looked up
   * @return std::list containing ManagedServer objects
   */
  std::list<fabric_cache::ManagedServer> group_lookup(const std::string &group_id);

  /** @brief Returns list of managed servers using sharding table and key
   *
   * Returns list of managed servers using sharding table and key.
   *
   * @param table_name The string representing the table name being sharded.
   * @param shard_key The shard key that needs to be looked up.
   * @return std::list containing ManagedServer objects
   */
  std::list<fabric_cache::ManagedServer> shard_lookup(const std::string &table_name, const std::string &shard_key);

private:
  enum shard_type_enum_ {
    RANGE, RANGE_INTEGER, RANGE_DATETIME, RANGE_STRING,
    HASH
  };

  /**
   * Copy source shard to the destination shard.
   *
   * @param source_shard Source shard structure.
   * @param destn_shard Destination shard structure.
   */
  void copy(const fabric_cache::ManagedShard &source_shard, fabric_cache::ManagedShard &destn_shard);

  /** @brief Fetches all data from Fabric
   *
   * Fetches all data from Fabric and stores it internally.
   */
  void fetch_data();

  /** @brief Refreshes the cache
   *
   * Refreshes the cache.
   */
  void refresh();

  /** @brief Returns instance of key comparator for Sharding
   *
   * Returns instance of the appropriated key comparator for Sharding.
   *
   * @param shard_type The sharding type for which the keys need to be compared.
   * @return Comparator class implementation.
   */
  ValueComparator *fetch_value_comparator(std::string shard_type);

  std::map<std::string, std::list<fabric_cache::ManagedServer>> group_data_;
  std::map<std::string, std::list<fabric_cache::ManagedShard>> shard_data_;
  int ttl_;

  std::map<std::string, std::list<fabric_cache::ManagedServer>> group_data_temp_;
  std::map<std::string, std::list<fabric_cache::ManagedShard>> shard_data_temp_;

  static const std::map<std::string, int> shard_type_map_;

  bool terminate_;

  std::shared_ptr<FabricMetaData> fabric_meta_data_;

  std::thread refresh_thread_;

  std::mutex cache_refreshing_mutex_;
};

#endif // FABRIC_CACHE_FABRIC_CACHE_INCLUDED
