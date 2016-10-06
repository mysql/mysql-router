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
