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
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace mysql_protocol {

ErrorPacket::ErrorPacket(uint8_t sequence_id, uint16_t err_code, const std::string &err_msg,
                         const std::string &sql_state, uint32_t capabilities)
    : Packet(sequence_id, capabilities),
      code_(err_code),
      message_(err_msg),
      sql_state_(sql_state) {
  prepare_packet();
}

ErrorPacket::ErrorPacket(const std::vector<uint8_t> &buffer,
                         uint32_t capabilities) : Packet(buffer, capabilities) {
  parse_payload();
}

void ErrorPacket::prepare_packet() {
  assert(sql_state_.size() == 5);

  reset();

  // Error identifier byte
  add_int<uint8_t>(0xff);

  // error code
  add_int<uint16_t>(code_);

  // SQL State
  if (capability_flags_ > 0 && (capability_flags_ & kClientProtocol41)) {
    add_int<uint8_t>(0x23);
    if (sql_state_.size() != 5) {
      add("HY000");
    } else {
      add(sql_state_);
    }
  }

  // The message
  add(message_);

  // Update the payload size in the header
  update_packet_size();
}

void ErrorPacket::parse_payload() {
  bool prot41 = capability_flags_ > 0 && (capability_flags_ & kClientProtocol41);
  // Sanity checks
  if (!((*this)[4] == 0xff && (*this)[6])) {
    throw packet_error("Error packet marker 0xff not found");
  }
  // Check if SQLState is available when CLIENT_PROTOCOL_41 flag is set
  if (prot41 && (*this)[7] != 0x23) {
    throw packet_error("Error packet does not contain SQL state");
  }

  unsigned long pos = 5;
  code_ = get_int<uint16_t>(pos);
  pos += 2;
  if ((*this)[7] == 0x23) {
    // We get the SQLState even when CLIENT_PROTOCOL_41 flag was not set
    // This is needed in cases when the server sends an
    // error to the client instead of the handshake.
    sql_state_ = get_string(++pos, 5); // We skip 0x23
    pos += 5;
  } else {
    sql_state_ = "";
  }
  message_ = get_string(pos);
}

} // namespace mysql_protocol
