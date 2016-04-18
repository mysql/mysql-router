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
  bool operator == (const ManagedServer & s1, const ManagedServer & s2);
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
  ManagedServer ms1;
  ManagedServer ms2;
  ManagedServer ms3;
  ManagedServer ms4;
  ManagedServer ms5;
  ManagedServer ms6;

  /**
   * Server list for each group in the topology. Each server object
   * represents all relevant information about the server that is
   * part of the topology.
   */
  list<ManagedServer> group_1_list;
  list<ManagedServer> group_2_list;
  list<ManagedServer> group_3_list;

  /**
   * Shard objects represent the information about the shard in the topology.
   */
  ManagedShard shard1;
  ManagedShard shard2;

  list<ManagedShard> table_1_list;

  /**
   * The information about the HA topology being managed by Fabric.
   */
  map<string, list<ManagedServer>> group_map;

  /**
   * The information about the shards present in Fabric.
   */
  map<string, list<ManagedShard>> shard_map;

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
  MockFabric(const string &host, int port, const string &user,
         const string &password, int connection_timeout,
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
  map<string, list<ManagedServer>> fetch_servers();

  /**
   *
   * Returns relation as a std::map between shard ID and list of managed
   * servers.
   *
   * @return Map of shard ID, shard details pair.
   */
  map<string, list<ManagedShard>> fetch_shards();

  /**
   *
   * Returns a mock refresh interval.
   *
   * @return refresh interval of the Fabric cache.
   */
  int fetch_ttl();
};



#endif //MOCK_FABRIC_INCLUDED

