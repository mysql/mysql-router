/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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
 * start and manage a connection routing from clients and MySQL servers.
 *
 */

#include "protocol/base_protocol.h"
#include "router_config.h"
#include "destination.h"
#include "mysql/harness/filesystem.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/mysql_protocol.h"
#include "plugin_config.h"
#include "utils.h"
#include "mysqlrouter/routing.h"
namespace mysql_harness { class PluginFuncEnv; }

#include <array>
#include <atomic>
#include <iostream>
#include <map>
#include <memory>

#ifndef _WIN32
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <sys/socket.h>
#  include <unistd.h>
#else
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif


#ifdef _WIN32
#  ifdef routing_DEFINE_STATIC
#    define ROUTING_API
#  else
#    ifdef routing_EXPORTS
#      define ROUTING_API __declspec(dllexport)
#    else
#      define ROUTING_API __declspec(dllimport)
#    endif
#  endif
#else
#  define ROUTING_API
#endif

using std::string;
using mysqlrouter::URI;

/** @class MySQLRoutering
 *  @brief Manage Connections from clients to MySQL servers
 *
 *  The class MySQLRouter is used to start a service listening on a particular
 *  TCP port for incoming MySQL Client connection and route these to a MySQL
 *  Server.
 *
 *  Connection routing will not analyze or parse any MySQL package (except from
 *  those in the handshake phase to be able to discover invalid connection error)
 *  nor will it do any authentication. It will not handle errors from the MySQL
 *  server and not automatically recover. The client communicate through
 *  MySQL Router just like it would directly connecting.
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
 *   r.destination_connect_timeout = std::chrono::seconds(1);
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
   * @param mode access mode, RO or RW
   * @param port TCP port for listening for incoming connections
   * @param protocol protocol for the routing
   * @param bind_address bind_address Bind to particular IP address
   * @param named_socket Bind to Unix socket/Windows named pipe
   * @param route_name Name of connection routing (can be empty string)
   * @param max_connections Maximum allowed active connections
   * @param destination_connect_timeout Timeout trying to connect destination server
   * @param max_connect_errors Maximum connect or handshake errors per host
   * @param connect_timeout Timeout waiting for handshake response
   * @param net_buffer_length send/receive buffer size
   * @param socket_operations object handling the operations on network sockets
   */
  MySQLRouting(routing::AccessMode mode, uint16_t port,
               const Protocol::Type protocol,
               const string &bind_address = string{"0.0.0.0"},
               const mysql_harness::Path& named_socket = mysql_harness::Path(),
               const string &route_name = string{},
               int max_connections = routing::kDefaultMaxConnections,
               std::chrono::milliseconds destination_connect_timeout = routing::kDefaultDestinationConnectionTimeout,
               unsigned long long max_connect_errors = routing::kDefaultMaxConnectErrors,
               std::chrono::milliseconds connect_timeout = routing::kDefaultClientConnectTimeout,
               unsigned int net_buffer_length = routing::kDefaultNetBufferLength,
               routing::SocketOperationsBase *socket_operations = routing::SocketOperations::instance());

  ~MySQLRouting();

  /** @brief Starts the service and accept incoming connections
   *
   * Starts the connection routing service and start accepting incoming
   * MySQL client connections. Each connection will be further handled
   * in a separate thread.
   *
   * Throws std::runtime_error on errors.
   *
   */
  void start(mysql_harness::PluginFuncEnv* env);

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
  void set_destinations_from_csv(const std::string &csv);

  void set_destinations_from_uri(const mysqlrouter::URI &uri);

  /** @brief Descriptive name of the connection routing */
  const std::string name;

  /** @brief Returns timeout when connecting to destination
   *
   * @return Timeout in seconds as int
   */
  std::chrono::milliseconds get_destination_connect_timeout() const noexcept {
    return destination_connect_timeout_;
  }

  /** @brief Sets timeout when connecting to destination
   *
   * Sets timeout connecting with destination servers.
   *
   * Throws std::invalid_argument when an invalid value was provided.
   *
   * @param timeout Timeout
   * @return New value as int
   */
  std::chrono::milliseconds set_destination_connect_timeout(std::chrono::milliseconds timeout);

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

  /** @brief Checks and if needed, blocks a host from using this routing
   *
   * Blocks a host from using this routing adding its IP address to the
   * list of blocked hosts when the maximum client errors has been
   * reached. Each call of this function will increment the number of
   * times it was called with the client IP address.
   *
   * When a client host is actually blocked, true will be returned,
   * otherwise false.
   *
   * @param client_ip_array IP address as array[16] of uint8_t
   * @param client_ip_str IP address as string (for logging purposes)
   * @param server Server file descriptor to wish to send
   *               fake handshake reply (default is not to send anything)
   * @return bool
   */
  bool block_client_host(const std::array<uint8_t, 16> &client_ip_array,
                         const std::string &client_ip_str, int server = -1);

  /** @brief Returns list of blocked client hosts
   *
   * Returns list of the blocked client hosts.
   */
  const std::vector<std::array<uint8_t, 16>> get_blocked_client_hosts() const;

  /** @brief Returns maximum active connections
   *
   * @return Maximum as int
   */
  int get_max_connections() const noexcept {
    return max_connections_;
  }

private:
  /** @brief Sets up the TCP service
   *
   * Sets up the TCP service binding to IP addresses and TCP port.
   *
   * Throws std::runtime_error on errors.
   *
   * @return
   */
  void setup_tcp_service();

  /** @brief Sets up the named socket service
   *
   * Sets up the named socket service creating a socket file on UNIX systems.
   *
   * Throws std::runtime_error on errors.
   */
  void setup_named_socket_service();

  /** @brief Worker function for thread
   *
   * Worker function handling incoming connection from a MySQL client using
   * the select-method.
   *
   * Errors are logged.
   *
   * @param client socket descriptor fo the client connection
   * @param client_addr IP address as sockaddr_storage struct
   */
  void routing_select_thread(int client, const sockaddr_storage &client_addr) noexcept;

  void start_acceptor(mysql_harness::PluginFuncEnv* env);

  /** @brief return a short string suitable to be used as a thread name
   * @param config_name configuration name (e.g: "routing", "routing:test_default_x_ro", etc)
   * @param prefix thread name prefix (e.g. "RtS")
   *
   * @return a short string (example: "RtS:x_ro")
   */
  static std::string make_thread_name(const std::string& config_name, const std::string& prefix);

  /** @brief Mode to use when getting next destination */
  routing::AccessMode mode_;
  /** @brief Maximum active connections
   *
   * Maximum number of incoming connections that will be accepted
   * by this MySQLRouter instances. There is no maximum for outgoing
   * connections since it is one-to-one with incoming.
   */
  int max_connections_;
  /** @brief Timeout connecting to destination
   *
   * This timeout is used when trying to connect with a destination
   * server. When the timeout is reached, another server will be
   * tried. It is good to leave this time out to 1 second or higher
   * if using an unstable network.
   */
  std::chrono::milliseconds destination_connect_timeout_;
  /** @brief Max connect errors blocking hosts when handshake not completed */
  unsigned long long max_connect_errors_;
  /** @brief Timeout waiting for handshake response from client */
  std::chrono::milliseconds client_connect_timeout_;
  /** @brief Size of buffer to store receiving packets */
  unsigned int net_buffer_length_;
  /** @brief IP address and TCP port for setting up TCP service */
  const mysqlrouter::TCPAddress bind_address_;
  /** @brief Path to named socket for setting up named socket service */
  const mysql_harness::Path bind_named_socket_;
  /** @brief Socket descriptor of the TCP service */
  int service_tcp_;
  /** @brief Socket descriptor of the named socket service */
  int service_named_socket_;
  /** @brief Destination object to use when getting next connection */
  std::unique_ptr<RouteDestination> destination_;
  /** @brief Number of active routes */
  std::atomic<uint16_t> info_active_routes_;
  /** @brief Number of handled routes */
  std::atomic<uint64_t> info_handled_routes_;

  /** @brief Connection error counters for IPv4 or IPv6 hosts */
  mutable std::mutex mutex_conn_errors_;
  std::map<std::array<uint8_t, 16>, size_t> conn_error_counters_;

  /** @brief TCP (and UNIX socket) service thread */
  std::thread thread_acceptor_;
  /** @brief object handling the operations on network sockets */
  routing::SocketOperationsBase* socket_operations_;
  /** @brief object to handle protocol specific stuff */
  std::unique_ptr<BaseProtocol> protocol_;

#ifdef FRIEND_TEST
  FRIEND_TEST(RoutingTests, bug_24841281);
  FRIEND_TEST(RoutingTests, make_thread_name);
  FRIEND_TEST(ClassicProtocolRoutingTest, NoValidDestinations);
  FRIEND_TEST(TestSetupTcpService, single_addr_ok);
  FRIEND_TEST(TestSetupTcpService, getaddrinfo_fails);
  FRIEND_TEST(TestSetupTcpService, socket_fails_for_all_addr);
  FRIEND_TEST(TestSetupTcpService, socket_fails);
  FRIEND_TEST(TestSetupTcpService, bind_fails);
  FRIEND_TEST(TestSetupTcpService, listen_fails);
#ifndef _WIN32
  FRIEND_TEST(TestSetupTcpService, setsockopt_fails);
#endif
#endif
};

extern "C"
{
  extern mysql_harness::Plugin ROUTING_API harness_plugin_routing;
}

#endif // ROUTING_MYSQLROUTING_INCLUDED
