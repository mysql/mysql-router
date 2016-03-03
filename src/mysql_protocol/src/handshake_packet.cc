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

#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/utils.h"

#include <cassert>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace mysql_protocol {

const unsigned int HandshakeResponsePacket::kDefaultClientCapabilities = 238221;

HandshakeResponsePacket::HandshakeResponsePacket(uint8_t sequence_id,
                                                 std::vector<unsigned char> auth_data,
                                                 const std::string &username,
                                                 const std::string &password, const std::string &database,
                                                 unsigned char char_set,
                                                 const std::string &auth_plugin)
    : Packet(sequence_id), auth_data_(auth_data), username_(username), password_(password),
      database_(database), char_set_(char_set), auth_plugin_(auth_plugin) {
  prepare_packet();
}

/** @fn HandshakeResponsePacket::prepare_packet()
 *
 * @devnote
 * Password is currently not used and 'incorrect' authentication data is
 * being set in this packet (making the packet currently unusable for authentication.
 * This is to satisfy fix for BUG22020088.
 * @enddevnote
 */
void HandshakeResponsePacket::prepare_packet() {

  reset();

  // capabilities
  add_int<uint32_t>(kDefaultClientCapabilities);

  // max packet size
  add_int<uint32_t>(kMaxAllowedSize);

  // Character set
  add_int<uint8_t>(char_set_);

  // Filler
  insert(end(), 23, 0x0);

  // Username
  if (!username_.empty()) {
    add(username_);
  }
  push_back(0x0);

  // Auth Data
  add_int<uint8_t>(20);
  insert(end(), 20, 0x71);  // 0x71 is fake data; can be anything

  // Database
  if (!database_.empty()) {
    add(database_);
  }
  push_back(0x0);

  // Authentication plugin name
  add(auth_plugin_);
  push_back(0x0);

  update_packet_size();
}

} // namespace mysql_protocol
