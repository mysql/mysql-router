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
#ifndef MOCK_METADATA_INCLUDED
#define MOCK_METADATA_INCLUDED

#include <vector>

#include "farm_metadata.h"

#include "mysqlrouter/datatypes.h"

namespace metadata_cache {
 /**
  * Compare two server objects, found in the metadata cache.
  */
  bool operator == (const metadata_cache::ManagedInstance & s1, const metadata_cache::ManagedInstance & s2);
 }

/** @class MockNG
 *
 * Used for simulating NG metadata for testing purposes.
 *
 */

class MockNG : public FarmMetadata {
public:
  /**
   * Objects representing the servers that are part of the topology.
   */
  metadata_cache::ManagedInstance ms1;
  metadata_cache::ManagedInstance ms2;
  metadata_cache::ManagedInstance ms3;
  metadata_cache::ManagedInstance ms4;
  metadata_cache::ManagedInstance ms5;
  metadata_cache::ManagedInstance ms6;
  metadata_cache::ManagedInstance ms7;
  metadata_cache::ManagedInstance ms8;
  metadata_cache::ManagedInstance ms9;

  /**
   * Server list for each replicaset in the topology. Each server object
   * represents all relevant information about the server that is
   * part of the topology.
   */
  std::vector<metadata_cache::ManagedInstance> replicaset_1_vector;
  std::vector<metadata_cache::ManagedInstance> replicaset_2_vector;
  std::vector<metadata_cache::ManagedInstance> replicaset_3_vector;

  /**
   * The information about the HA topology being managed.
   */
  std::map<std::string, std::vector<metadata_cache::ManagedInstance>> replicaset_map;

  /** @brief Constructor
   * @param user The user name used to authenticate to the metadata server.
   * @param password The password used to authenticate to the metadata server.
   * @param connection_timeout The time after which a connection to the
   *                           metadata server should timeout.
   * @param connection_attempts The number of times a connection to the metadata
   *                            server must be attempted, when a connection
   *                            attempt fails.
   * @param ttl The time to live of the data in the cache.
   */
  MockNG(const std::string &user, const std::string &password,
         int connection_timeout, int connection_attempts, unsigned int ttl);

  /** @brief Destructor
   *
   * Disconnect and release the connection to the metadata node.
   */
  ~MockNG();

  /** @brief Mock connect method.
   *
   * Mock connect method, does nothing.
   *
   * @return a boolean to indicate if the connection was successful.
   */
  bool connect(const std::vector<metadata_cache::ManagedInstance> &
               metadata_servers) noexcept;

  /** @brief Mock disconnect method.
   *
   * Mock method, does nothing.
   *
   */
  void disconnect() noexcept;

  /**
   *
   * Returns relation as a std::map between replicaset ID and list of managed
   * servers.
   *
   * @return Map of replicaset ID, server list pairs.
   */
  std::map<std::string, std::vector<metadata_cache::ManagedInstance>> fetch_instances();

  /**
   *
   * Returns a mock refresh interval.
   *
   * @return refresh interval of the Metadata cache.
   */
  unsigned int fetch_ttl();
};



#endif //MOCK_METADATA_INCLUDED
