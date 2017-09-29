/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLD_MOCK_MYSQL_PROTOCOL_COMMON_INCLUDED
#define MYSQLD_MOCK_MYSQL_PROTOCOL_COMMON_INCLUDED

#include <string>

namespace server_mock {

using byte = uint8_t;

/** @enum MySQLColumnType
 *
 * Supported MySQL Coumn types.
 *
 **/
enum class MySQLColumnType {
  TINY =  0x01,
  LONG = 0x03,
  LONGLONG = 0x08,
  STRING = 0xfe
};

/** @brief Struct for keeping column specific data
 *
 **/
struct column_info_type {
  std::string name;
  MySQLColumnType type;
  std::string orig_name;
  std::string table;
  std::string orig_table;
  std::string schema;
  std::string catalog;
  uint16_t flags;
  uint8_t decimals;
  uint32_t length;
  uint16_t character_set;

  unsigned repeat;
};

/** @brief Vector for keeping string representation of the values
 *         of the single row (ordered by column)
 **/
using row_values_type = std::vector<std::string>;

MySQLColumnType column_type_from_string(const std::string& type);

} // namespace

#endif // MYSQLD_MOCK_MYSQL_PROTOCOL_COMMON_INCLUDED
