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

class Protocol final {
public:
  using Type = BaseProtocol::Type;

  static inline Type get_default() {
    return Type::kClassicProtocol;
  }

  /** @brief Returns type of the protocol by its name
   */
  static Type get_by_name(const std::string &name) {
    Type result = Type::kClassicProtocol;

    if (name == "classic") {
      /* already set */
    }
    else if (name == "x") {
      result = Type::kXProtocol;
    }
    else {
      throw std::invalid_argument("Invalid protocol name: '" + name + "'");
    }

    return result;
  }

  /** @brief Factory method creating protocol object for handling the routing code that is protocol-specific
   *
   * @param type type of the protocol for which the handler should be created
   * @param socket_operations socket operations
   *
   * @returns pointer to the created object
   */
  static BaseProtocol* create(Type type,
                              SocketOperationsBase *socket_operations) {
    BaseProtocol* result{nullptr};

    switch (type ) {
    case Type::kClassicProtocol:
      result =  new ClassicProtocol(socket_operations);
      break;
    case Type::kXProtocol:
      result =  new XProtocol(socket_operations);
      break;
    default:
      throw std::invalid_argument("Invalid protocol: " + std::to_string(static_cast<int>(type)));
    }

    return result;
  }

  /** @brief Returns default port for the selected protocol
   */
  static uint16_t get_default_port(Type type) {
    uint16_t result{0};

    switch (type ) {
    case Type::kClassicProtocol:
      result = kClassicProtocolDefaultPort;
      break;
    case Type::kXProtocol:
      result = kXProtocolDefaultPort;
      break;
    default:
      throw std::invalid_argument("Invalid protocol: " + std::to_string(static_cast<int>(type)));
    }

    return result;
  }

private:
  /** @brief default server ports for supported protocols */
  static constexpr uint16_t kClassicProtocolDefaultPort{3306};
  static constexpr uint16_t kXProtocolDefaultPort{33060};
};

#endif // ROUTING_PROTOCOL_INCLUDED
