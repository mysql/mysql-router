/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLROUTER_MYSQL_PROTOCOL_HANDSHAKE_PACKET_INCLUDED
#define MYSQLROUTER_MYSQL_PROTOCOL_HANDSHAKE_PACKET_INCLUDED

#include "base_packet.h"

namespace mysql_protocol {

/** @class HandshakeResponsePacket
 * @brief Creates a MySQL handshake response packet
 *
 * This class creates a MySQL handshake response packet which is send by
 * the MySQL client after receiving the server's handshake packet.
 *
 */
class MYSQL_PROTOCOL_API HandshakeResponsePacket final : public Packet {
 public:
  /** @brief Default capability flags
   *
   * Default capability flags, including:
   *
   * * CLIENT_LONG_PASSWD
   * * CLIENT_LONG_FLAG
   * * CLIENT_CONNECT_WITH_DB
   * * CLIENT_PROTOCOL_41
   * * CLIENT_TRANSACTIONS
   * * CLIENT_SECURE_CONNECTION
   * * CLIENT_MULTI_STATEMENTS
   * * CLIENT_MULTI_RESULTS
   * * CLIENT_LOCAL_FILES
   *
   */
  static const unsigned int kDefaultClientCapabilities;

  /** @brief Constructor */
  HandshakeResponsePacket() : Packet(0), auth_data_({}), username_(""), password_(""),
                              char_set_(8), auth_plugin_("mysql_native_password") {
    prepare_packet();
  }

  /** @overload
   *
   * @param sequence_id MySQL Packet number
   * @param auth_data Authentication data from the MySQL server handshake
   * @param username MySQL username to use
   * @param password MySQL password to use
   * @param database MySQL database to use when connecting (default is empty)
   * @param char_set MySQL character set code (default 8, latin1)
   * @param auth_plugin MySQL authentication plugin name (default 'mysql_native_password')
   */
  HandshakeResponsePacket(uint8_t sequence_id,
                          std::vector<unsigned char> auth_data, const std::string &username,
                          const std::string &password, const std::string &database = "",
                          unsigned char char_set = 8,
                          const std::string &auth_plugin = "mysql_native_password");

 private:
  /** @brief Prepares the packet
   *
   * Prepares the actual MySQL Error packet and stores it. The header is
   * created using the sequence id and the size of the payload.
   */
  void prepare_packet();

  /** @brief Authentication data provided by the MySQL handshake packet */
  std::vector<unsigned char> auth_data_;

  /** @brief MySQL username */
  std::string username_;

  /** @brief MySQL password */
  std::string password_;

  /** @brief MySQL database */
  std::string database_;

  /** @brief MySQL character set */
  unsigned char char_set_;

  /** @brief MySQL authentication plugin name */
  std::string auth_plugin_;
};

} // namespace mysql_protocol

#endif // MYSQLROUTER_MYSQL_PROTOCOL_HANDSHAKE_PACKET_INCLUDED
