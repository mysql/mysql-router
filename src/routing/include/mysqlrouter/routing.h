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

#ifndef MYSQLROUTER_ROUTING_INCLUDED
#define MYSQLROUTER_ROUTING_INCLUDED

#include "mysqlrouter/datatypes.h"

#include <map>
#include <string>

using std::string;

namespace routing {

/** @brief Timeout for idling clients (in seconds)
 *
 * Constant defining how long (in seconds) a client can keep the connection idling. This is similar to the
 * wait_timeout variable in the MySQL Server.
 */
extern const int kDefaultWaitTimeout;

/** @brief Max number of active routes for this routing instance */
extern const int kDefaultMaxConnections;

/** @brief Timeout connecting to destination (in seconds)
 *
 * Constant defining how long we wait to establish connection with the server before we give up.
 */
extern const int kDefaultDestinationConnectionTimeout;

/** @brief Maximum connect or handshake errors per host
 *
 * Maximum connect or handshake errors after which a host will be
 * blocked. Such errors can happen when the client does not reply
 * the handshake, sends an incorrect packet, or garbage.
 *
 */
extern const unsigned long long kDefaultMaxConnectErrors;

/** @brief Default bind address
 *
 */
extern const string kDefaultBindAddress;

/** @brief Default net buffer length
 *
 * Default network buffer length which can be set in the MySQL Server.
 *
 * This should match the default of the latest MySQL Server.
 */
extern const unsigned int kDefaultNetBufferLength;

/** @brief Timeout waiting for handshake response from client
 *
 * The number of seconds that MySQL Router waits for a handshake response.
 * The default value is 9 seconds (default MySQL Server minus 1).
 *
 */
extern const unsigned int kDefaultClientConnectTimeout;

/** @brief Modes supported by Routing plugin */
enum class AccessMode {
  kReadWrite = 1,
  kReadOnly = 2,
};

/** @brief Literal name for each Access Mode */
extern const std::map<string, AccessMode> kAccessModeNames;

/** @brief Returns literal name of given access mode
 *
 * Returns literal name of given access mode as a std:string. When
 * the access mode is not found, empty string is returned.
 *
 * @param access_mode Access mode to look up
 * @return Name of access mode as std::string or empty string
 */
string get_access_mode_name(AccessMode access_mode) noexcept;

/**
 * Sets blocking flag for given socket
 *
 * @param sock a socket file descriptor
 * @param blocking whether to set blocking off (false) or on (true)
 */
void set_socket_blocking(int sock, bool blocking);

/** @brief Returns socket descriptor of connected MySQL server
 *
 * Returns a socket descriptor for the connection to the MySQL Server or
 * -1 when an error occurred.
 *
 * @param addr information of the server we connect with
 * @param connect_timeout number of seconds waiting for connection
 * @param log whether to log errors or not
 * @return a socket descriptor
 */
int get_mysql_socket(mysqlrouter::TCPAddress addr, int connect_timeout, bool log = true) noexcept;

}

#endif // MYSQLROUTER_ROUTING_INCLUDED
