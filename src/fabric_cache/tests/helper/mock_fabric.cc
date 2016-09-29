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

#include "mock_fabric.h"

#include <map>
#include <memory>
#include <list>

#include "mysqlrouter/fabric_cache.h"


using namespace std;

namespace fabric_cache {

/**
 * Compare two server objects, managed by fabric.
 */
bool operator == (const ManagedServer & s1, const ManagedServer & s2) {
  return (s1.server_uuid == s2.server_uuid &&
          s1.group_id == s2.group_id &&
          s1.host == s2.host &&
          s1.port == s2.port &&
          s1.mode == s2.mode &&
          s1.status == s2.status &&
          s1.weight == s2.weight);
}

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
MockFabric::MockFabric(const string &host, int port, const string &user,
           const string &password, int connection_timeout,
           int connection_attempts) : Fabric(host, port, user, password,
                                             connection_timeout,
                                             connection_attempts) {
  ms1.server_uuid = "UUID1";
  ms1.group_id = "group-1";
  ms1.host = "host-1";
  ms1.port = 3306;
  ms1.mode = 3;
  ms1.status = 3;
  ms1.weight = 1;

  ms2.server_uuid = "UUID2";
  ms2.group_id = "group-1";
  ms2.host = "host-2";
  ms2.port = 3307;
  ms2.mode = 1;
  ms2.status = 2;
  ms2.weight = 1;

  ms3.server_uuid = "UUID3";
  ms3.group_id = "group-2";
  ms3.host = "host-3";
  ms3.port = 3306;
  ms3.mode = 3;
  ms3.status = 3;
  ms3.weight = 1;

  ms4.server_uuid = "UUID4";
  ms4.group_id = "group-2";
  ms4.host = "host-4";
  ms4.port = 3307;
  ms4.mode = 1;
  ms4.status = 2;
  ms4.weight = 1;

  ms5.server_uuid = "UUID5";
  ms5.group_id = "group-3";
  ms5.host = "host-5";
  ms5.port = 3306;
  ms5.mode = 3;
  ms5.status = 3;
  ms5.weight = 1;

  ms6.server_uuid = "UUID6";
  ms6.group_id = "group-3";
  ms6.host = "host-6";
  ms6.port = 3307;
  ms6.mode = 1;
  ms6.status = 2;
  ms6.weight = 1;

  group_1_list.push_back(ms1);
  group_1_list.push_back(ms2);

  group_2_list.push_back(ms3);
  group_2_list.push_back(ms4);

  group_3_list.push_back(ms5);
  group_3_list.push_back(ms6);

  group_map["group-1"] = group_1_list;
  group_map["group-2"] = group_2_list;
  group_map["group-3"] = group_3_list;

  shard1.schema_name = "db1";
  shard1.table_name = "t1";
  shard1.column_name = "empno";
  shard1.lb = "1";
  shard1.shard_id = 1;
  shard1.type_name = "RANGE_INTEGER";
  shard1.group_id = "group-2";
  shard1.global_group = "group-1";

  shard2.schema_name = "db1";
  shard2.table_name = "t1";
  shard2.column_name = "empno";
  shard2.lb = "1000";
  shard2.shard_id = 2;
  shard2.type_name = "RANGE_INTEGER";
  shard2.group_id = "group-3";
  shard2.global_group = "group-1";

  table_1_list.push_back(shard1);
  table_1_list.push_back(shard2);

  shard_map["db1.t1"] = table_1_list;
}

/** @brief Destructor
 *
 * Disconnect and release the connection to the fabric node.
 */
MockFabric::~MockFabric() {}

/** @brief Returns relation between group ID and list of servers
 *
 * Returns relation as a std::map between group ID and list of managed servers.
 *
 * @return Map of group ID, server list pairs.
 */
map<string, list<ManagedServer>> MockFabric::fetch_servers() {
  return group_map;
}

/** @brief Mock connect method.
 *
 * Mock connect method, does nothing.
 *
 * @return a boolean to indicate if the connection was successful.
 */
bool MockFabric::connect() noexcept {
  return true;
}

/** @brief Mock connect method.
 *
 * Mock connect method, does nothing.
 *
 * @return a boolean to indicate if the connection was successful.
 */
void MockFabric::disconnect() noexcept {
}

/**
 *
 * Returns relation as a std::map between shard ID and list of managed
 * servers.
 *
 * @return Map of shard ID, shard details pair.
 */
map<string, list<ManagedShard>> MockFabric::fetch_shards() {
  return shard_map;
}

/**
 *
 * Returns a mock refresh interval.
 *
 * @return refresh interval of the Fabric cache.
 */
int MockFabric::fetch_ttl() {
  return 5;
}

}  // namespace fabric_cache
