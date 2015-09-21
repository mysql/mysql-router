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

#ifndef MYSQLROUTER_ROUTING_INCLUDED
#define MYSQLROUTER_ROUTING_INCLUDED

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

}

#endif // MYSQLROUTER_ROUTING_INCLUDED
