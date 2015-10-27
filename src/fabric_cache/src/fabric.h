/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef FABRIC_CACHE_FABRIC_INCLUDED
#define FABRIC_CACHE_FABRIC_INCLUDED

#include "mysqlrouter/fabric_cache.h"
#include "fabric_metadata.h"
#include "utils.h"

#include <list>
#include <map>
#include <string>

#include <mysql.h>
#include <string.h>

using std::string;

/** @class Fabric
 *
 * The `Fabric` class encapsulates a connection to the Fabric server. It
 * uses the MySQL Client C Library to setup, manage and retrieve results.
 *
 */
class Fabric : public FabricMetaData {
public:
  /** @brief Constructor
   * @param host The host on which the fabric server is running.
   * @param port The port number on which the fabric server is listening.
   * @param user The user name used to authenticate to the fabric server.
   * @param password The password used to authenticate to the fabric server.
   * @param connection_timeout The time after which a connection to the
   *                           fabric server should timeout.
   * @param connection_attempts The number of times a connection to fabric must be
   *                            attempted, when a connection attempt fails.
   *
   */
  Fabric(const string &host, int port, const string &user,
         const string &password, int connection_timeout,
         int connection_attempts);

  /** @brief Destructor
   *
   * Disconnect and release the connection to the fabric node.
   */
  ~Fabric();


  /** @brief Returns relation between group ID and list of servers
   *
   * Returns relation as a std::map between group ID and list of managed servers.
   *
   * @return Map of group ID, server list pairs.
   */
  map<string, list<ManagedServer>> fetch_servers();

  /** @brief Returns relation between shard ID and list of servers
   *
   * Returns relation as a std::map between shard ID and list of managed servers.
   *
   * @return Map of shard ID, shard details pair.
   */
  map<string, list<ManagedShard>> fetch_shards();

  /** @brief Returns the refresh interval provided by Fabric
   *
   * Returns the refresh interval (also known as TTL) provided by Fabric.
   *
   * @return refresh interval of the Fabric cache.
   */
  int fetch_ttl();

  /** @brief Connects with the Fabric server
   *
   * Checks first whether we are connected. If not, this method will
   * try indefinitely try to reconnect with the Fabric server.
   *
   * @return a boolean to indicate if the connection was successful.
   */
  bool connect() noexcept;

  /** @brief Disconnects from the Fabric server
   *
   * Checks first whether we are connected. If not, this method will
   * try indefinitely try to reconnect with the Fabric server.
   */
  void disconnect() noexcept;
  
private:
  /** @brief Returns result from remote API call
   *
   * Returns result from remote API call executed on the Fabric Server.
   *
   * @param remote_api Remote API to be executed
   * @return MYSQL_RES object containg result of remote API execution
   */
  MYSQL_RES *fetch_metadata(string &remote_api);

  // Fabric node connection information
  string host_;
  int port_;
  string user_;
  string password_;

  // Fabric node generic information
  string fabric_uuid_;
  int ttl_;
  string message_;

  // The time after which a connection to the fabric server should timeout.
  int connection_timeout_;

  // The number of times we should try connecting to fabric if a connection attempt fails.
  int connection_attempts_;

  // MySQL client objects
  MYSQL *fabric_connection_;

  // Boolean variable indicates if a connection to fabric has been established.
  bool connected_ = false;

  // How many times we tried to reconnected (for logging purposes)
  size_t reconnect_tries_;
};

#endif // FABRIC_CACHE_FABRIC_INCLUDED
