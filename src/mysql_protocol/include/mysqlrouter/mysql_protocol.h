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

#ifndef MYSQLROUTER_MYSQL_PROTOCOL_INCLUDED
#define MYSQLROUTER_MYSQL_PROTOCOL_INCLUDED

#include <cassert>
#include <cmath>
#include <cstddef>
#include <string>
#include <typeinfo>
#include <vector>

#include "mysql_protocol/constants.h" // comes first
#include "mysql_protocol/base_packet.h"
#include "mysql_protocol/error_packet.h"
#include "mysql_protocol/handshake_packet.h"

namespace mysql_protocol {

/** @class packet_error
 * @brief Exception raised for any errors with MySQL packets
 *
 */
class packet_error : public std::runtime_error {
  public:
    explicit packet_error(const std::string &what_arg) : std::runtime_error(what_arg) { }
};

} // namespace mysql_protocol

#endif // MYSQLROUTER_MYSQL_PROTOCOL_INCLUDED
