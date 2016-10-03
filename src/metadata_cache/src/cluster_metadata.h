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

#ifndef METADATA_CACHE_METADATA_INCLUDED
#define METADATA_CACHE_METADATA_INCLUDED

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/metadata_cache.h"
#include "metadata.h"

#include <vector>
#include <map>
#include <string>

#include <mysql.h>
#include <string.h>

struct GroupReplicationMember;

/** @class ClusterMetadata
 *
 * The `ClusterMetadata` class encapsulates a connection to the Metadata server. It
 * uses the MySQL Client C Library to setup, manage and retrieve results.
 *
 */
class METADATA_API ClusterMetadata : public MetaData {
public:
  /** @brief Constructor
   *
   * @param user The user name used to authenticate to the metadata server.
   * @param password The password used to authenticate to the metadata server.
   * @param connection_timeout The time after which a connection to the
   *                           metadata server should timeout.
   * @param connection_attempts The number of times a connection to metadata
   *                            must be attempted, when a connection attempt
   *                            fails.
   * @param ttl The time to live of the data in the cache.
   */
  ClusterMetadata(const std::string &user, const std::string &password,
               int connection_timeout, int connection_attempts,
               unsigned int ttl);

  /** @brief Destructor
   *
   * Disconnect and release the connection to the metadata node.
   */
  virtual ~ClusterMetadata();


  /** @brief Returns relation between replicaset ID and list of servers
   *
   * Returns relation as a std::map between replicaset ID and list of managed servers.
   *
   * @param cluster_name the name of the cluster to query
   * @return Map of replicaset ID, server list pairs.
   */
  InstancesByReplicaSet fetch_instances(const std::string &cluster_name) override;

  /** @brief Returns the refresh interval provided by the metadata server.
   *
   * Returns the refresh interval (also known as TTL) provided by metadata server.
   *
   * @return refresh interval of the Metadata cache.
   */
  unsigned int fetch_ttl() override;

  /** @brief Connects with the Metadata server
   *
   * Checks first whether we are connected. If not, this method will
   * try indefinitely try to reconnect with the Metadata server.
   *
   * @param metadata_servers the set of servers from which the metadata
   *                         information is fetched.
   *
   * @return a boolean to indicate if the connection was successful.
   */
  bool connect(const std::vector<metadata_cache::ManagedInstance> &
               metadata_servers) noexcept override;

  /** @brief Disconnects from the Metadata server
   *
   * Checks first whether we are connected. If not, this method will
   * try indefinitely try to reconnect with the Metadata server.
   */
  void disconnect() noexcept override;

private:
  /** Connects a MYSQL connection descriptor to the given instance
   */
  bool do_connect(MYSQL *mysql, const metadata_cache::ManagedInstance &mi);

  /** @brief Returns result from a query.
   *
   * Returns result from a query executed on the Metadata Server.
   *
   * @param query Query to be executed
   * @return MYSQL_RES object containg result of remote API execution
   */
  MYSQL_RES *run_query(const std::string &query);

  /** @brief Queries the metadata server for the list of instances and
   * replicasets that belong to the desired cluster.
   */
  InstancesByReplicaSet fetch_instances_from_metadata_server(const std::string &cluster_name);

  void update_replicaset_status(const std::string &name,
      std::vector<metadata_cache::ManagedInstance> &instances);

  metadata_cache::ReplicasetStatus check_replicaset_status(
      std::vector<metadata_cache::ManagedInstance> &instances,
      std::map<std::string, GroupReplicationMember> &member_status);

  // Metadata node connection information
  std::string user_;
  std::string password_;

  // Metadata node generic information
  std::string metadata_uuid_;
  unsigned int ttl_;
  std::string cluster_name_;
  std::string message_;

  // The time after which a connection to the metadata server should timeout.
  int connection_timeout_;

  // The number of times we should try connecting to the metadata server if a
  // connection attempt fails.
  int connection_attempts_;

  // MySQL client objects
  MYSQL *metadata_connection_;

  // The address of the instance metadata_connection_ is connected to
  std::string metadata_connection_address_;

  // Boolean variable indicates if a connection to metadata has been established.
  bool connected_ = false;

  // How many times we tried to reconnected (for logging purposes)
  size_t reconnect_tries_;
};

#endif // METADATA_CACHE_METADATA_INCLUDED
