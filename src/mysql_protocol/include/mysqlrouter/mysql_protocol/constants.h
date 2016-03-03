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

#ifndef MYSQLROUTER_MYSQL_PROTOCOL_CONSTANTS_INCLUDED
#define MYSQLROUTER_MYSQL_PROTOCOL_CONSTANTS_INCLUDED

namespace mysql_protocol {

// Capability flags are prefixed with `CLIENT_`.
// - See https://dev.mysql.com/doc/internals/en/capability-flags.html
// - See also MySQL Server source include/mysql_com.h
// - using uint32_t because transmitted as 4 byte long integer

/** @brief CLIENT_PROTOCOL_41
 *
 * Server: Supports the 4.1 protocol.
 * Client: Uses the 4.1 protocol.
 */
const uint32_t kClientProtocol41 = 0x00000200;

/** @brief CLIENT_SSL
 *
 * Server: Supports SSL.
 * Client: Switch to SSL.
 */
const uint32_t kClientSSL = 0x00000800;

} // mysql_protocol

#endif // MYSQLROUTER_MYSQL_PROTOCOL_CONSTANTS_INCLUDED
