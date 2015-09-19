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

#ifndef MYSQLROUTER_UTILS_INCLUDED
#define MYSQLROUTER_UTILS_INCLUDED

#include <cassert>
#include <cstdarg>
#include <netdb.h>
#include <sstream>
#include <string>
#include <vector>

namespace mysqlrouter {

using std::string;

// Some (older) compiler have no std::to_string avialable 
template<typename T>
std::string to_string(const T &data) {
  std::ostringstream os;
  os << data;
  return os.str();
}

/** @brief Returns string formatted using given data
*
* Returns string formatted using given data accepting the same arguments
* and format specifies as the typical printf.
*
* @param format specify how to format the data
* @param ... variable argument list containing the data
* @returns formatted text as string
*/
string string_format(const char *format, ...);

/**
 * Split host and port
 *
 * @param data a string with hostname and port
 * @return std::pair<string, uint16_t> containing address and port
 */
std::pair<string, uint16_t> split_addr_port(const string data);

/**
 * Validates a string containing a TCP port
 *
 * Validates whether the data can be used as a TCP port. A TCP port is
 * a valid number in the range of 0 and 65535. The returned integer is
 * of type uint16_t.
 *
 * An empty data string will result in TCP port 0 to be returned.
 *
 * Throws runtime_error when the given string can not be converted
 * to an integer or when the integer is to big.
 *
 * @param data string containing the TCP port number
 * @return uint16_t the TCP port number
 */
uint16_t get_tcp_port(const string& data);

} // namespace mysqlrouter

#endif // MYSQLROUTER_UTILS_INCLUDED
