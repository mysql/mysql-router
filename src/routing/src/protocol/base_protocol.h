/*
Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTING_BASEPROTOCOL_INCLUDED
#define ROUTING_BASEPROTOCOL_INCLUDED

#include <cstdint>
#include <string>
#include <vector>
#include "mysqlrouter/mysql_protocol.h"

#ifndef _WIN32
#include <unistd.h>
#else
#include <winsock2.h>
#undef ERROR
#endif

using RoutingProtocolBuffer = mysql_protocol::Packet::vector_t;

namespace routing {
  class SocketOperationsBase;
}

using routing::SocketOperationsBase;

class BaseProtocol {
public:

  /** @brief supported protocols */
  enum class Type {
    kClassicProtocol,
    kXProtocol
  };

  BaseProtocol(SocketOperationsBase *socket_operations): socket_operations_(socket_operations) {}
  virtual ~BaseProtocol() {}

  /** @brief Function that gets called when the client is being blocked
   *
   * This function is called when the client is being blocked and should handle
   * any communication to the server required by the protocol in such case
   *
   * @param server Descriptor of the server
   * @param log_prefix prefix to be used by the function as a tag for logging
   *
   * @return true on success; false on error
   */
  virtual bool on_block_client_host(int server, const std::string &log_prefix) = 0;

  /** @brief Reads from sender and writes it back to receiver using select
   *
   * This function reads data from the sender socket and writes it back
   * to the receiver socket. It use `select`.
   *
   * @param sender Descriptor of the sender
   * @param receiver Descriptor of the receiver
   * @param readfds Read descriptors used with FD_ISSET
   * @param buffer Buffer to use for storage
   * @param curr_pktnr Pointer to storage for sequence id of packet
   * @param handshake_done Whether handshake phase is finished or not
   * @param report_bytes_read Pointer to storage to report bytes read
   * @param from_server true if the message sender is the server, false
   *                    if it is a client
   *
   * @return 0 on success; -1 on error
   */
  virtual int copy_packets(int sender, int receiver, fd_set *readfds,
                           RoutingProtocolBuffer &buffer, int *curr_pktnr,
                           bool &handshake_done, size_t *report_bytes_read,
                           bool from_server) = 0;

  /** @brief Sends error message to the provided receiver.
   *
   * This function sends protocol message containing MySQL error
   *
   * @param destination descriptor of the receiver
   * @param code general error code
   * @param message human readable error message
   * @param sql_state SQL state for the error
   * @param log_prefix prefix to be used by the function as a tag for logging
   *
   * @return true on success; false on error
   */
  virtual bool send_error(int destination,
                          unsigned short code,
                          const std::string &message,
                          const std::string &sql_state,
                          const std::string &log_prefix) = 0;

  /** @brief Gets protocol type. */
  virtual Type get_type() = 0;
protected:
  SocketOperationsBase *socket_operations_;
};

#endif // ROUTING_BASEPROTOCOL_INCLUDED
