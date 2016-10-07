/*
Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "common.h"
#include "logger.h"
#include "x_protocol.h"
#include "../utils.h"
#include "mysqlrouter/routing.h"

#include "mysqlx.pb.h"
#include <google/protobuf/io/coded_stream.h>

#include <algorithm>
#include <cassert>

using ProtobufMessage = google::protobuf::Message;

const size_t kMessageHeaderSize = 5;

static bool send_message(const std::string &log_prefix,
                         int destination,
                         int8_t type,
                         const ProtobufMessage &msg,
                         SocketOperationsBase *socket_operations) {
  using google::protobuf::io::CodedOutputStream;

  const size_t msg_size = msg.ByteSize();
  RoutingProtocolBuffer buffer(kMessageHeaderSize + msg_size);

  // first 4 bytes is the message size (plus type byte, without size bytes)
  CodedOutputStream::WriteLittleEndian32ToArray(static_cast<uint32_t>(msg_size + 1), &buffer[0]);
  // fifth byte is the message type
  buffer[kMessageHeaderSize -1] = static_cast<uint8_t>(type);

  if (!msg.SerializeToArray(&buffer[kMessageHeaderSize], msg.ByteSize())) {
    log_debug("[%s] error while serializing error message: %s", log_prefix.c_str());
    return false;
  }

  if (socket_operations->write_all(destination, &buffer[0], buffer.size()) < 0) {
    log_debug("[%s] write error: %s", log_prefix.c_str(), get_message_error(errno).c_str());
    return false;
  }

  return true;
}

static bool get_next_message(int sender,
                             RoutingProtocolBuffer &buffer,
                             size_t   &buffer_contents_size,
                             size_t   &message_offset,
                             int8_t   &message_type,
                             uint32_t &message_size,
                             SocketOperationsBase *socket_operations,
                             bool &error) {
  using google::protobuf::io::CodedInputStream;
  error = false;
  ssize_t read_res = 0;

  assert(buffer_contents_size >= message_offset);
  size_t bytes_left = buffer_contents_size - message_offset;

  // no more messages to process
  if (bytes_left == 0) {
    return false;
  }

  // we need at least 4 bytes to know the message size
  while (bytes_left < 4) {
    errno = 0;
#ifdef _WIN32
    WSASetLastError(0);
#endif
    read_res = socket_operations->read(sender, &buffer[message_offset + bytes_left], 4 - bytes_left);
    if (read_res <= 0) {
      log_error("failed reading size of the message: (%d %s %d)", errno, get_message_error(errno).c_str(), read_res);
      error = true;
      return false;
    }

    buffer_contents_size += read_res;
    bytes_left += read_res;
  }

  // we got the message size, we can decode it
  CodedInputStream::ReadLittleEndian32FromArray(&buffer[message_offset], &message_size);

  // If not the whole message is in the buffer we need to read the remaining part to be able to decode it.
  // First let's check if the message will fit the buffer.
  // Currently we decode the messages ONLY in the handshake phase when we expect relatively small messages:
  // (AuthOk, AutCont, Notice, Error, CapabilitiesGet...)
  // In case the message does not fit the buffer, we just return an error. This way we defend against the possibility
  // of the client sending huge messages while authenticating.
  size_t size_needed = message_offset + 4 + message_size;
  if (buffer.size() < size_needed) {
    log_error("X protocol message too big to fit the buffer: (%u, %u, %u)", message_size, buffer.size(), message_offset);
    error = true;
    return false;
  }
  // next read the remaining part of the message if needed
  while (message_size + 4 > bytes_left) {
    errno = 0;
#ifdef _WIN32
    WSASetLastError(0);
#endif
    read_res = socket_operations->read(sender, &buffer[message_offset+bytes_left], message_size + 4 - bytes_left);
    if (read_res <= 0) {
      log_error("failed reading part of X protocol message: (%d %s %d)", errno, get_message_error(errno).c_str(), read_res);
      error = true;
      return false;
    }

    buffer_contents_size += read_res;
    bytes_left += read_res;
  }

  message_type = buffer[message_offset + kMessageHeaderSize - 1];

  return true;
}

template <typename T>
static T any_as_integral_type(const Mysqlx::Datatypes::Any &any) {
  using namespace Mysqlx::Datatypes;

  assert(any.type() == Mysqlx::Datatypes::Any::SCALAR);
  const Scalar &scalar = any.scalar();

  T result;
  switch (scalar.type())
  {
  case Scalar::V_BOOL:
    result = static_cast<T>(scalar.v_bool());
    break;

  case Scalar::V_SINT:
    result = static_cast<T>(scalar.v_signed_int());
    break;

  default:
    assert(scalar.type() == Scalar::V_UINT);
    result = static_cast<T>(scalar.v_unsigned_int());
  }

  return result;
}

static bool ssl_requested(const Mysqlx::Connection::CapabilitiesSet& capabilites_msg) {
  for (int i = 0; i < capabilites_msg.capabilities().capabilities_size(); ++i) {
     auto &capability = capabilites_msg.capabilities().capabilities(i);
     std::string cap_name = capability.name();
     std::transform(cap_name.begin(), cap_name.end(), cap_name.begin(), ::tolower);
     if (cap_name == "tls") {
       if (any_as_integral_type<bool>(capability.value())) {
         return true;
       }
       break;
     }
   }

  return false;
}

int XProtocol::copy_packets(int sender, int receiver, fd_set *readfds,
                            RoutingProtocolBuffer &buffer, int * /*curr_pktnr*/,
                            bool &handshake_done, size_t *report_bytes_read,
                            bool from_server) {
  assert(readfds != nullptr);
  assert(report_bytes_read != nullptr);

  ssize_t res = 0;
  auto buffer_length = buffer.size();
  size_t bytes_read = 0;

  errno = 0;
#ifdef _WIN32
  WSASetLastError(0);
#endif
  if (FD_ISSET(sender, readfds)) {
    if ((res = socket_operations_->read(sender, &buffer.front(), buffer_length)) <= 0) {
      if (res == -1) {
        log_error("sender read failed: (%d %s)", errno, get_message_error(errno).c_str());
      }
      return -1;
    }
    errno = 0;
#ifdef _WIN32
    WSASetLastError(0);
#endif
    bytes_read += static_cast<size_t>(res);
    if (!handshake_done) {
      // Check packets integrity when handshaking. When message SessionAuthenticateOK
      // is sent from the server, then we assume handshaking is satisfied. For secure
      // connections, we stop when client asks to switch to SSL.

      int8_t message_type;
      size_t message_offset = 0;
      uint32_t message_size = 0;
      // the buffer can contain partial message or more than one message
      // the loop is to make sure that all messages are inspected
      // and that the whole message that is being processed is in the buffer
      bool msg_read_error = false;
      while (get_next_message(sender, buffer, bytes_read, message_offset, message_type,
                              message_size, socket_operations_, msg_read_error)
             && !msg_read_error) {

        // if the client requests SSL during the handshake we will not be able to decode
        // further communication so we have to assume handshake is done
        if (!from_server && message_type == Mysqlx::ClientMessages::CON_CAPABILITIES_SET) {
          Mysqlx::Connection::CapabilitiesSet capabilites_msg;
          int msg_size = static_cast<int>(bytes_read-(message_offset+ kMessageHeaderSize));
          if (!capabilites_msg.ParseFromArray(&buffer[message_offset+ kMessageHeaderSize],
                                              msg_size)) {
            log_error("Error parsing X Protocol capabilities message");
            return -1;
          }

          if (ssl_requested(capabilites_msg)) {
            handshake_done = true;
            break;
          }
        }

        // if server confirmed client's authentication the handshake is done
        if (from_server && message_type == Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK) {
          handshake_done = true;
          break;
        }

        message_offset += (message_size + 4);
      }

      if (msg_read_error) {
        return -1;
      }
    }

    if (socket_operations_->write_all(receiver, &buffer[0], bytes_read) < 0) {
      log_error("Write error: %s", get_message_error(errno).c_str());
      return -1;
    }
  }
  *report_bytes_read = bytes_read;

  return 0;
}

bool XProtocol::send_error(int destination,
                           unsigned short code,
                           const std::string &message,
                           const std::string &sql_state,
                           const std::string &log_prefix) {
  Mysqlx::Error error;
  error.set_code(code);
  error.set_sql_state(sql_state);
  error.set_msg(message);

  return send_message(log_prefix, destination, Mysqlx::ServerMessages::ERROR, error, socket_operations_);
}
