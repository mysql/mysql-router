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
#ifndef ROUTING_PROTOCOL_INCLUDED
#define ROUTING_PROTOCOL_INCLUDED

#include "base_protocol.h"
#include "classic_protocol.h"
#include "x_protocol.h"

#include <cassert>
#include <memory>
#include <set>

class Protocol final {
public:
  /** @brief default protocol name */
  static constexpr const char* DEFAULT{"classic"};

  /** @brief default protocol name */
  static const std::set<std::string> get_supported_protocols() {
    return {"classic", "x"};
  }

  /** @brief Returns default port for the selected protocol
   *
   * @param name name of the protocol
   *
   * @returns default port for the protocol
   */
  static uint16_t get_default_port(const std::string &name) {
    if (name == "classic") {
      return 3306;
    }

    assert(name=="x");
    return 33060;
  }

  /** @brief Factory method creating object for handling the routing code that is protocol-specific
   *
   * @param name name of the protocol for which the handler should be created
   *
   * @returns pointer to the created object
   */
  static BaseProtocol* create_protocol(const std::string &name,
                                       SocketOperationsBase *socket_operations) {
    if (name == "classic") {
      return new ClassicProtocol(socket_operations);
    }

    assert(name=="x");
    return new XProtocol(socket_operations);
  }
};

#endif // ROUTING_PROTOCOL_INCLUDED
