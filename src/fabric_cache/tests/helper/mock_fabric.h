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
#ifndef MOCK_FABRIC_INCLUDED
#define MOCK_FABRIC_INCLUDED

#include "fabric.h"

namespace fabric_cache {
 /**
  * Compare two server objects, managed by fabric.
  */
  bool operator == (const fabric_cache::ManagedServer & s1, const fabric_cache::ManagedServer & s2);
 }

/** @class MockFabric
 *
 * Used for simulating fabric metadata for testing purposes.
 *
 */

class MockFabric : public Fabric {
public:
  /**
   * Objects representing the servers that are part of the topology.
   */
  fabric_cache::ManagedServer ms1;
  fabric_cache::ManagedServer ms2;
  fabric_cache::ManagedServer ms3;
  fabric_cache::ManagedServer ms4;
  fabric_cache::ManagedServer ms5;
  fabric_cache::ManagedServer ms6;

  /**
   * Server list for each group in the topology. Each server object
   * represents all relevant information about the server that is
   * part of the topology.
   */
  std::list<fabric_cache::ManagedServer> group_1_list;
  std::list<fabric_cache::ManagedServer> group_2_list;
  std::list<fabric_cache::ManagedServer> group_3_list;

  /**
   * Shard objects represent the information about the shard in the topology.
   */
  fabric_cache::ManagedShard shard1;
  fabric_cache::ManagedShard shard2;

  std::list<fabric_cache::ManagedShard> table_1_list;

  /**
   * The information about the HA topology being managed by Fabric.
   */
  std::map<std::string, std::list<fabric_cache::ManagedServer>> group_map;

  /**
   * The information about the shards present in Fabric.
   */
  std::map<std::string, std::list<fabric_cache::ManagedShard>> shard_map;

  /** @brief Constructor
   * @param host The host on which the fabric server is running.
   * @param port The port number on which the fabric server is listening.
   * @param user The user name used to authenticate to the fabric server.
   * @param password The password used to authenticate to the fabric server.
   * @param connection_timeout The time after which a connection to the
   *                           fabric server should timeout.
   * @param connection_attempts The number of times a connection to fabric must
   *                            be attempted, when a connection attempt fails.
   *
   */
  MockFabric(const std::string &host, int port, const std::string &user,
         const std::string &password, int connection_timeout,
             int connection_attempts);

  /** @brief Destructor
   *
   * Disconnect and release the connection to the fabric node.
   */
  ~MockFabric();

  /** @brief Mock connect method.
   *
   * Mock connect method, does nothing.
   *
   * @return a boolean to indicate if the connection was successful.
   */
  bool connect() noexcept;

  /** @brief Mock disconnect method.
   *
   * Mock method, does nothing.
   *
   */
  void disconnect() noexcept;

  /**
   *
   * Returns relation as a std::map between group ID and list of managed
   * servers.
   *
   * @return Map of group ID, server list pairs.
   */
  std::map<std::string, std::list<fabric_cache::ManagedServer>> fetch_servers();

  /**
   *
   * Returns relation as a std::map between shard ID and list of managed
   * servers.
   *
   * @return Map of shard ID, shard details pair.
   */
  std::map<std::string, std::list<fabric_cache::ManagedShard>> fetch_shards();

  /**
   *
   * Returns a mock refresh interval.
   *
   * @return refresh interval of the Fabric cache.
   */
  int fetch_ttl();
};



#endif //MOCK_FABRIC_INCLUDED

