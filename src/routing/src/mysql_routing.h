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

#ifndef ROUTING_MYSQLROUTING_INCLUDED
#define ROUTING_MYSQLROUTING_INCLUDED

/** @file
 * @brief Defining the class MySQLRouting
 *
 * This file defines the main class `MySQLRouting` which is used to configure,
 * start and manage a conenction routing from clients and MySQL servers.
 *
 */

#include "utils.h"
#include "destination.h"
#include "config.h"

#include "plugin_config.h"

#include <atomic>
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <netinet/tcp.h>
#include <memory>
#include <map>

#include "mysqlrouter/datatypes.h"

using std::string;

/** @class MySQLRoutering
 *  @brief Manage Connections from clients to MySQL servers
 *
 *  The class MySQLRouter is used to start a service listening on a particular
 *  TCP port for incoming MySQL Client connection and route these to a MySQL
 *  Server.
 *
 *  Conenction routing will not analyze or parse any MySQL package nor will
 *  it do any authentication. It will not handle errors from the MySQL server
 *  and not automatically recover. The client communicate through MySQL Router
 *  just like it would directly connecting.
 *
 *  The MySQL Server is chosen from a given list of hosts or IP addresses
 *  (with or without TCP port) based on the the mode. For example, mode
 *  read-only will go through the list of servers in a round-robin way. The
 *  mode read-write will always go through the list from the beginning and
 *  failover to the next available.
 *
 *
 *  Example usage: bind to all IP addresses and use TCP Port 7001
 *
 *   MySQLRouting r(routing::AccessMode::kReadWrite, "0.0.0.0", 7001);
 *   r.wait_timeout = 200;
 *   r.destination_connect_timeout = 1;
 *   r.set_destinations_from_csv("10.0.10.5;10.0.11.6");
 *   r.start();
 *
 *  The above example will, when MySQL running on 10.0.10.5 is not available,
 *  use 10.0.11.6 to setup the connection routing.
 *
 */
class MySQLRouting {
public:
  /** @brief Default constructor
   *
   * @param port TCP port for listening for incoming connections
   * @param optional bind_address bind_address Bind to particular IP address
   * @param optional route Name of connection routing (can be empty string)
   * @param optional max_connections Maximum allowed active connections
   * @param optional wait_timeout Timeout after which idle clients are disconnected
   * @param optional destination_connect_timeout Timeout trying to connect destination server
   */
  MySQLRouting(routing::AccessMode mode, int port, const string &bind_address = string{"0.0.0.0"},
               const string &route_name = string{},
               int max_connections = routing::kDefaultMaxConnections,
               int wait_timeout = routing::kDefaultWaitTimeout,
               int destination_connect_timeout = routing::kDefaultDestinationConnectionTimeout);

  /** @brief Starts the service and accept incoming connections
   *
   * Starts the connection routing service and start accepting incoming
   * MySQL client connections. Each connection will be further handled
   * in a separate thread.
   *
   * Throws std::runtime_error on errors.
   *
   */
  void start();

  /** @brief Asks the service to stop
   *
   */
  void stop();

  /** @brief Returns whether the service is stopping
   *
   * @return a bool
   */
  bool stopping() {
    return stopping_.load();
  }

  /** @brief Sets the destinations from URI
   *
   * Sets destinations using the given string and the given mode. The string
   * should be a comma separated list of MySQL servers.
   *
   * The mode is one of MySQLRouting::Mode, for example MySQLRouting::Mode::kReadOnly.
   *
   * Example of destinations:
   *   "10.0.10.5,10.0.11.6:3307"
   *
   * @param csv destinations as comma-separated-values
   */
  void set_destinations_from_csv(const string &csv);

  void set_destinations_from_uri(const URI &uri);

  /** @brief Descriptive name of the connection routing */
  const string name;

  /** @brief Returns timeout for idling client connection
   *
   * @return Timeout in seconds as int
   */
  int get_wait_timeout() const noexcept {
    return wait_timeout_;
  }

  /** @brief Sets timeout for idling client connection
   *
   * Sets timeout for idling clients. Timeout in seconds must be between 1 and
   * 65535.
   *
   * Throws std::invalid_argument when an invalid value was provided.
   *
   * @param seconds Timeout in seconds
   * @return New value as int
   */
  int set_wait_timeout(int seconds);

  /** @brief Returns timeout when connecting to destination
   *
   * @return Timeout in seconds as int
   */
  int get_destination_connect_timeout() const noexcept {
    return destination_connect_timeout_;
  }

  /** @brief Sets timeout when connecting to destination
   *
   * Sets timeout connecting with destination servers. Timeout in seconds must be between 1 and
   * 65535.
   *
   * Throws std::invalid_argument when an invalid value was provided.
   *
   * @param seconds Timeout in seconds
   * @return New value as int
   */
  int set_destination_connect_timeout(int seconds);

  /** @brief Sets maximum active connections
   *
   * Sets maximum of active connections. Maximum must be between 1 and
   * 65535.
   *
   * Throws std::invalid_argument when an invalid value was provided.
   *
   * @param maximum Max number of connections allowed
   * @return New value as int
   */
  int set_max_connections(int maximum);

  /** @brief Returns maximum active connections
   *
   * @return Maximum as int
   */
  int get_max_connections() const noexcept {
    return max_connections_;
  }

private:
  /** @brief Returns socket descriptor for next MySQL server
   *
   * Gets information about the next server in the destination
   * list, tries to conenct and returns the socket server.
   *
   * Errors are logged and not raised.
   *
   * @return a socket descriptor
   */
  int get_destination() noexcept;

  /** @brief Returns socket descriptor of conencted MySQL server
   *
   * Returns a socket descriptor for the connection to the MySQL Server or
   * zero when an error occured.
   *
   * @param addr information of the server we connect with
   * @return a socket descriptor
   */
  int get_mysql_connection(mysqlrouter::TCPAddress addr) noexcept;

  /** @brief Sets up the service
   *
   * Sets up the service binding to IP addresses and TCP port.
   *
   * Throws std::runtime_error on errors.
   *
   */
  void setup_service();

  /** @brief Worker function for thread
   *
   * Worker function handling incoming connection from a MySQL client using
   * the select-method.
   *
   * Errors are logged.
   *
   * @param client socket descriptor fo the client connection
   * @param timeout timeout in seconds
   */
  void thd_routing_select(int client) noexcept;

  /** @brief Mode to use when getting next destination */
  routing::AccessMode mode_;
  /** @brief Maximum active connections
   *
   * Maximum number of incoming connections that will be accepted
   * by this MySQLRouter instances. There is no maximum for outgoing
   * connections since it is one-to-one with incoming.
   */
  int max_connections_;
  /** @brief Timeout for idling clients
   *
   * The number of seconds we wait for activity on a connection before
   * closing it. Leaving this option relatively low will help in
   * saving resources, but this depends on the applications using Router.
   */
  int wait_timeout_;
  /** @brief Timeout connecting to destination
   *
   * This timeout is used when trying to connect with a destination
   * server. When the timeout is reached, another server will be
   * tried. It is good to leave this time out to 1 second or higher
   * if using an unstable network.
   */
  int destination_connect_timeout_;
  /** @brief IP address and TCP port to use when binding service */
  const TCPAddress bind_address_;
  /** @brief Socket descriptor of the service */
  int sock_server_;
  /** @brief Destination object to use when getting next connection */
  std::unique_ptr<RouteDestination> destination_;
  /** @brief Whether we were asked to stop */
  std::atomic<bool> stopping_;
  /** @brief Number of active routes */
  std::atomic<uint16_t> info_active_routes_;
  /** @brief Number of handled routes */
  std::atomic<uint64_t> info_handled_routes_;
};


#endif // ROUTING_MYSQLROUTING_INCLUDED
