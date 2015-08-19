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

class MySQLRouting;
class RoutingPluginConfig;

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
 *   MySQLRouting r("0.0.0.0", 7001);
 *   r.wait_timeout = 200;
 *   r.destination_connect_timeout = 1;
 *   r.set_destination("10.0.10.5;10.0.11.6", routing::AccessMode::kReadWrite);
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
   * @param port TCP port for listining for incoming connections
   * @param optional bind_address Bind to particular IP address
   * @param optional route name
   */
  MySQLRouting(uint16_t port, const string &bind_address, const string &route_name);

  /** @overload */
  MySQLRouting(uint16_t port, const string &bind_address) : MySQLRouting(port, bind_address, "") { }

  /** @overload */
  MySQLRouting(uint16_t port) : MySQLRouting(port, "0.0.0.0", "") { }

  /** @overload */
  MySQLRouting(const RoutingPluginConfig &config);

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

  /** @brief Sets the destination
   *
   * Sets the destination using the given string and the given mode. The string
   * can be either a semicolon dist of MySQL servers or a valid supported URI.
   *
   * The mode is one of MySQLRouting::Mode, for example MySQLRouting::Mode::kReadOnly.
   *
   * Example of destination:
   *   "10.0.10.5;10.0.11.6:3307"
   *
   * @param destination destination as string provided by configuration
   * @param mode mode to use, for example read-only or read-write
   */
  void set_destination(const string &destination, const routing::AccessMode mode);

  /** @brief Returns the name of the current mode
   *
   *  Returns the name of the current mode as a string.
   *
   *  @return name of the configured mode
   */
  string get_mode_name();

  /** @brief Descriptive name of the connection routing */
  const string name;

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

  /** @brief Timeout for idling clients */
  int wait_timeout_;
  /** @brief Timeout connecting to destination */
  int destination_connect_timeout_;
  /** @brief IP address and TCP port to use when binding service */
  const TCPAddress bind_address_;
  /** @brief Socket descriptor of the service */
  int sock_server_;
  /** @brief Mode to use when getting next destination */
  routing::AccessMode mode_;
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
