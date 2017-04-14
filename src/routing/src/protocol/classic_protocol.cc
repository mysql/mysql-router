/*
Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "classic_protocol.h"

#include "common.h"
#include "mysql/harness/logging.h"
#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/routing.h"
#include "../utils.h"

#include <cstring>

using mysql_harness::get_strerror;
IMPORT_LOG_FUNCTIONS()

bool ClassicProtocol::on_block_client_host(int server, const std::string &log_prefix) {
  auto fake_response = mysql_protocol::HandshakeResponsePacket(1, {}, "ROUTER", "", "fake_router_login");
  if (socket_operations_->write_all(server, fake_response.data(), fake_response.size()) < 0) {
    log_debug("[%s] write error: %s", log_prefix.c_str(), get_message_error(errno).c_str());
    return false;
  }
  return true;
}

int ClassicProtocol::copy_packets(int sender, int receiver, fd_set *readfds,
                                  RoutingProtocolBuffer &buffer, int *curr_pktnr,
                                  bool &handshake_done, size_t *report_bytes_read,
                                  bool /*from_server*/) {
  assert(curr_pktnr);
  assert(report_bytes_read);
  ssize_t res = 0;
  int pktnr = 0;
  auto buffer_length = buffer.size();

  size_t bytes_read = 0;

  if (!handshake_done && *curr_pktnr == 2) {
    handshake_done = true;
  }

  errno = 0;
#ifdef _WIN32
  WSASetLastError(0);
#endif
  if (FD_ISSET(sender, readfds)) {
    if ((res = socket_operations_->read(sender, &buffer.front(), buffer_length)) <= 0) {
      if (res == -1) {
        log_debug("sender read failed: (%d %s)", errno, get_message_error(errno).c_str());
      }
      return -1;
    }
    errno = 0;
#ifdef _WIN32
    WSASetLastError(0);
#endif
    bytes_read += static_cast<size_t>(res);
    if (!handshake_done) {
      // Check packet integrity when handshaking. When packet number is 2, then we assume
      // handshaking is satisfied. For secure connections, we stop when client asks to
      // switch to SSL.
      // The caller should set handshake_done to true when packet number is 2.
      if (bytes_read < mysql_protocol::Packet::kHeaderSize) {
        // We need packet which is at least 4 bytes
        return -1;
      }
      pktnr = buffer[3];
      if (*curr_pktnr > 0 && pktnr != *curr_pktnr + 1) {
        log_debug("Received incorrect packet number; aborting (was %d)", pktnr);
        return -1;
      }

      if (buffer[4] == 0xff) {
        // We got error from MySQL Server while handshaking
        // We do not consider this a failed handshake

        // copy part of the buffer containing serialized error
        RoutingProtocolBuffer buffer_err(buffer.begin(), buffer.begin() +
                                         static_cast<RoutingProtocolBuffer::iterator::difference_type>(bytes_read));

        auto server_error = mysql_protocol::ErrorPacket(buffer_err);
        if (socket_operations_->write_all(receiver, server_error.data(), server_error.size()) < 0) {
          log_debug("Write error: %s", get_message_error(errno).c_str());
        }
        // receiver socket closed by caller
        *curr_pktnr = 2; // we assume handshaking is done though there was an error
        *report_bytes_read = bytes_read;
        return 0;
      }

      // We are dealing with the handshake response from client
      if (pktnr == 1) {
        // if client is switching to SSL, we are not continuing any checks
        uint32_t capabilities = 0;
        try {
          auto pkt = mysql_protocol::Packet(buffer);
          capabilities = pkt.get_int<uint32_t>(4);
        } catch (const mysql_protocol::packet_error &exc) {
          log_debug(exc.what());
          return -1;
        }
        if (capabilities & mysql_protocol::kClientSSL) {
          pktnr = 2;  // Setting to 2, we tell the caller that handshaking is done
        }
      }
    }

    if (socket_operations_->write_all(receiver, &buffer[0], bytes_read) < 0) {
      log_debug("Write error: %s", get_message_error(errno).c_str());
      return -1;
    }
  }

  *curr_pktnr = pktnr;
  *report_bytes_read = bytes_read;

  return 0;
}

bool ClassicProtocol::send_error(int destination,
                                 unsigned short code,
                                 const std::string &message,
                                 const std::string &sql_state,
                                 const std::string &log_prefix) {
  auto server_error = mysql_protocol::ErrorPacket(0, code, message, sql_state);
  errno = 0;
#ifdef _WIN32
  WSASetLastError(0);
#endif
  if (socket_operations_->write_all(destination, server_error.data(), server_error.size()) < 0) {
    log_debug("[%s] write error: %s", log_prefix.c_str(), get_message_error(errno).c_str());
  }
  return errno == 0;
}
