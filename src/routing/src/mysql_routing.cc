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

#include "mysql_routing.h"
#include "dest_first_available.h"
#include "dest_fabric_cache.h"
#include "mysqlrouter/uri.h"
#include "plugin_config.h"
#include "mysqlrouter/routing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <sys/fcntl.h>
#include <sys/types.h>
#undef __FD_SETSIZE
#define __FD_SETSIZE 4096
#include <sys/select.h>
#include <string>

#include "mysqlrouter/fabric_cache.h"
#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/utils.h"
#include "logger.h"

using std::runtime_error;
using mysqlrouter::string_format;
using mysqlrouter::to_string;
using routing::AccessMode;
using mysqlrouter::URI;
using mysqlrouter::URIError;
using mysqlrouter::URIQuery;


MySQLRouting::MySQLRouting(routing::AccessMode mode, int port, const string &bind_address,
                           const string &route_name,
                           int max_connections,
                           int destination_connect_timeout,
                           unsigned long long max_connect_errors,
                           unsigned int client_connect_timeout,
                           unsigned int net_buffer_length)
    : name(route_name),
      mode_(mode),
      max_connections_(set_max_connections(max_connections)),
      destination_connect_timeout_(set_destination_connect_timeout(destination_connect_timeout)),
      max_connect_errors_(max_connect_errors),
      client_connect_timeout_(client_connect_timeout),
      net_buffer_length_(net_buffer_length),
      bind_address_(TCPAddress(bind_address, port)),
      stopping_(false),
      info_active_routes_(0),
      info_handled_routes_(0) {
  if (!bind_address_.port) {
    throw std::invalid_argument(string_format("Invalid bind address, was '%s', port %d", bind_address.c_str(), port));
  }
}

/** @brief Reads from sender and writes it back to receiver using select
 *
 * This function reads data from the sender socket and writes it back
 * to the receiver socket. It use `select`.
 *
 * Checking the handshaking is done when the client first connects and
 * the server sends its handshake. The client replies and the server
 * should reply with an OK (or Error) packet. This packet should be
 * packet number 2. For secure connections, however, the client asks
 * to switch to SSL and we can not check further packages (we can not
 * decrypt). When SSL switch is detected, this function will set pktnr
 * to 2, so we assume the handshaking was OK.
 *
 * @param sender Descriptor of the sender
 * @param receiver Descriptor of the receiver
 * @param readfds Read descriptors used with FD_ISSET
 * @param buffer Buffer to use for storage
 * @param curr_pktnr Pointer to storage for sequence id of packet
 * @param handshake_done Whether handshake phase is finished or not
 * @param report_bytes_read Pointer to storage to report bytes read
 * @return 0 on success; -1 on error
 */
int copy_mysql_protocol_packets(int sender, int receiver, fd_set *readfds,
                                mysql_protocol::Packet::vector_t &buffer, int *curr_pktnr,
                                bool handshake_done, size_t *report_bytes_read) {
  assert(curr_pktnr);
  assert(report_bytes_read);
  ssize_t res = 0;
  int pktnr = 0;
  auto buffer_length = buffer.size();

  size_t bytes_read = 0;

  if (FD_ISSET(sender, readfds)) {
    if ((res = read(sender, &buffer.front(), buffer_length)) <= 0) {
      if (res == -1) {
        log_debug("sender read failed: (%d %s)", errno, strerror(errno));
      }
      return -1;
    }
    errno = 0;
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
        auto server_error = mysql_protocol::ErrorPacket(buffer);
        write(receiver, server_error.data(), server_error.size());
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
    size_t bytes_to_write = bytes_read;
    ssize_t written = 0;
    while (bytes_to_write > 0) {
      if ((written = write(receiver, buffer.data(), bytes_to_write)) < 0) {
        log_debug("Write error: %s", strerror(errno));
        return -1;
      }
      bytes_to_write -= static_cast<size_t>(written);
    }
  }

  *curr_pktnr = pktnr;
  *report_bytes_read = bytes_read;

  return 0;
}

bool MySQLRouting::block_client_host(const std::array<uint8_t, 16> &client_ip_array,
                                     const string &client_ip_str, int server) {
  bool blocked = false;
  std::lock_guard<std::mutex> lock(mutex_auth_errors_);

  if (++auth_error_counters_[client_ip_array] >= max_connect_errors_) {
    log_warning("[%s] blocking client host %s", name.c_str(), client_ip_str.c_str());
    blocked = true;
  } else {
    log_info("[%s] %d authentication errors for %s (max %d)",
             name.c_str(), auth_error_counters_[client_ip_array], client_ip_str.c_str(), max_connect_errors_);
  }

  if (server >= 0) {
    auto fake_response = mysql_protocol::HandshakeResponsePacket(1, {}, "ROUTER", "", "fake_router_login");
    write(server, fake_response.data(), fake_response.size());
  }

  return blocked;
}

void MySQLRouting::routing_select_thread(int client, const in6_addr client_addr) noexcept {
  int nfds;
  int res;
  struct timeval timeout_val;
  int error = 0;
  size_t bytes_down = 0;
  size_t bytes_up = 0;
  size_t bytes_read = 0;
  string extra_msg = "";
  mysql_protocol::Packet::vector_t buffer(net_buffer_length_);
  bool handshake_done = false;

  int server = destination_->get_server_socket(destination_connect_timeout_, &error);

  if (!(server > 0 && client > 0)) {
    std::stringstream os;
    os << "Can't connect to MySQL server on ";
    os << "'" << bind_address_.addr << "'";
    auto server_error = mysql_protocol::ErrorPacket(0, 2003, os.str(), "HY000");
    write(client, server_error.data(), server_error.size());

    shutdown(client, SHUT_RDWR);
    shutdown(server, SHUT_RDWR);
    if (client > 0) {
      close(client);
    }
    if (server > 0) {
      close(server);
    }
    return;
  }

  auto c_ip = get_peer_name(client);
  auto s_ip = get_peer_name(server);

  log_debug("[%s] [%s]:%d - [%s]:%d", name.c_str(), c_ip.first.c_str(), c_ip.second,
            s_ip.first.c_str(), s_ip.second);
  ++info_handled_routes_;
  ++info_active_routes_;

  nfds = std::max(client, server) + 1;

  int pktnr = 0;
  while (true) {
    fd_set readfds;
    fd_set errfds;
    // Reset on each loop
    FD_ZERO(&readfds);
    FD_SET(client, &readfds);
    FD_SET(server, &readfds);

    if (handshake_done) {
      res = select(nfds, &readfds, nullptr, &errfds, nullptr);
    } else {
      // Handshake reply timeout
      struct timeval timeout_val;
      timeout_val.tv_sec = client_connect_timeout_;
      timeout_val.tv_usec = 0;
      res = select(nfds, &readfds, nullptr, &errfds, &timeout_val);
    }

    if (res <= 0) {
      if (res == 0) {
        extra_msg = string("Select timed out");
      } else if (errno > 0) {
        extra_msg = string("Select failed with error: " + to_string(strerror(errno)));
      } else {
        extra_msg = string("Select failed (" + to_string(res) + ")");
      }

      break;
    }

    if (!handshake_done && pktnr == 2) {
      handshake_done = true;
    }

    // Handle traffic from Server to Client
    // Note: Server _always_ talks first
    if (copy_mysql_protocol_packets(server, client,
                                    &readfds, buffer, &pktnr,
                                    handshake_done, &bytes_read) == -1) {
      if (errno > 0) {
        extra_msg = string("Copy server-client failed: " + to_string(strerror(errno)));
      }
      break;
    }
    bytes_up += bytes_read;

    if (!handshake_done && pktnr == 2) {
      handshake_done = true;
    }

    // Handle traffic from Client to Server
    if (copy_mysql_protocol_packets(client, server,
                                    &readfds, buffer, &pktnr,
                                    handshake_done, &bytes_read) == -1) {
      break;
    }
    bytes_down += bytes_read;

  }

  if (!handshake_done) {
    auto ip_array = in6_addr_to_array(client_addr);
    log_debug("[%s] Routing failed for %s: %s", name.c_str(), c_ip.first.c_str(), extra_msg.c_str());
    block_client_host(ip_array, c_ip.first.c_str(), server);
  }

  // Either client or server terminated
  shutdown(client, SHUT_RDWR);
  shutdown(server, SHUT_RDWR);
  close(client);
  close(server);
  --info_active_routes_;
  log_debug("[%s] Routing stopped (up:%zub;down:%zub) %s", name.c_str(), bytes_up, bytes_down, extra_msg.c_str());
}

void MySQLRouting::start() {
  int sock_client;
  struct sockaddr_in6 client_addr;
  socklen_t sin_size = sizeof client_addr;
  char client_ip[INET6_ADDRSTRLEN];
  int opt_nodelay = 1;

  try {
    setup_service();
  } catch (const runtime_error &exc) {
    throw runtime_error(
        string_format("Setting up service using %s: %s", bind_address_.str().c_str(), exc.what()));
  }

  log_info("[%s] listening on %s; %s", name.c_str(), bind_address_.str().c_str(),
           routing::get_access_mode_name(mode_).c_str());

  destination_->start();

  auto error_1041 = mysql_protocol::ErrorPacket(
      0, 1041, "Out of resources (please check logs)", "HY000");

  while (!stopping()) {
    if ((sock_client = accept(sock_server_, (struct sockaddr *) &client_addr, &sin_size)) < 0) {
      log_error("[%s] Failed opening socket: %s", name.c_str(), strerror(errno));
      continue;
    }

    if (inet_ntop(AF_INET6, &client_addr, client_ip, sizeof(client_ip)) == nullptr) {
      log_error("[%s] inet_ntop failed: %s", name.c_str(), strerror(errno));
      continue;
    }

    if (auth_error_counters_[in6_addr_to_array(client_addr.sin6_addr)] >= max_connect_errors_) {
      std::stringstream os;
      os << "Too many connection errors from " << get_peer_name(sock_client).first;
      auto server_error = mysql_protocol::ErrorPacket(0, 1129, os.str(), "HY000");
      write(sock_client, server_error.data(), server_error.size());
      close(sock_client); // no shutdown() before close()
      continue;
    }

    if (info_active_routes_.load(std::memory_order_relaxed) >= max_connections_) {
      auto server_error = mysql_protocol::ErrorPacket(0, 1040, "Too many connections", "HY000");
      write(sock_client, server_error.data(), server_error.size());
      close(sock_client); // no shutdown() before close()
      log_warning("[%s] reached max active connections (%d)", name.c_str(), max_connections_);
      continue;
    }

    if (setsockopt(sock_client, IPPROTO_TCP, TCP_NODELAY, &opt_nodelay, sizeof(int)) == -1) {
      log_error("[%s] client setsockopt error: %s", name.c_str(), strerror(errno));
      continue;
    }

    std::thread(&MySQLRouting::routing_select_thread, this, sock_client, client_addr.sin6_addr).detach();
  }

  log_info("[%s] stopped", name.c_str());
}

void MySQLRouting::stop() {
  stopping_.store(true);
}

void MySQLRouting::setup_service() {
  struct addrinfo *servinfo, *info, hints;
  int err;
  int option_value;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  errno = 0;

  err = getaddrinfo(bind_address_.addr.c_str(), to_string(bind_address_.port).c_str(), &hints, &servinfo);
  if (err != 0) {
    throw runtime_error(string_format("[%s] Failed getting address information (%s)",
                                      name.c_str(), gai_strerror(err)));
  }

  // Try to setup socket and bind
  for (info = servinfo; info != nullptr; info = info->ai_next) {
    if ((sock_server_ = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
      throw std::runtime_error(strerror(errno));
    }

    option_value = 1;
    if (setsockopt(sock_server_, SOL_SOCKET, SO_REUSEADDR, &option_value, sizeof(int)) == -1) {
      throw std::runtime_error(strerror(errno));
    }

    if (::bind(sock_server_, info->ai_addr, info->ai_addrlen) == -1) {
      close(sock_server_);
      throw std::runtime_error(strerror(errno));
    }
    break;
  }
  freeaddrinfo(servinfo);

  if (info == nullptr) {
    throw runtime_error(string_format("[%s] Failed to setup server socket", name.c_str()));
  }

  if (listen(sock_server_, 20) < 0) {
    throw runtime_error(string_format("[%s] Failed to start listening for connections", name.c_str()));
  }
}

void MySQLRouting::set_destinations_from_uri(const URI &uri) {
  if (uri.scheme == "fabric+cache") {
    auto fabric_cmd = uri.path[0];
    std::transform(fabric_cmd.begin(), fabric_cmd.end(), fabric_cmd.begin(), ::tolower);
    if (fabric_cmd == "group") {
      if (!fabric_cache::have_cache(uri.host)) {
        throw runtime_error("Invalid Fabric Cache in URI; was '" + uri.host + "'");
      }
      destination_.reset(new DestFabricCacheGroup(uri.host, uri.path[1], mode_, uri.query));
    } else {
      throw runtime_error("Invalid Fabric command in URI; was '" + fabric_cmd + "'");
    }
  } else {
    throw runtime_error(string_format("Invalid URI scheme '%s' for URI %s", uri.scheme.c_str()));
  }
}

void MySQLRouting::set_destinations_from_csv(const string &csv) {
  std::stringstream ss(csv);
  std::string part;
  std::pair<std::string, uint16_t> info;

  if (AccessMode::kReadOnly == mode_) {
    destination_.reset(new RouteDestination());
  } else if (AccessMode::kReadWrite == mode_) {
    destination_.reset(new DestFirstAvailable());
  } else {
    throw std::runtime_error("Unknown mode");
  }
  // Fall back to comma separated list of MySQL servers
  while (std::getline(ss, part, ',')) {
    info = mysqlrouter::split_addr_port(part);
    if (info.second == 0) {
      info.second = 3306;
    }
    TCPAddress addr(info.first, info.second);
    if (addr.is_valid()) {
      destination_->add(addr);
    } else {
      throw std::runtime_error(string_format("Destination address '%s' is invalid", addr.str().c_str()));
    }
  }

  // Check whether bind address is part of list of destinations
  for (auto &it: *destination_) {
    if (it == bind_address_) {
      throw std::runtime_error("Bind Address can not be part of destinations");
    }
  }

  if (destination_->size() == 0) {
    throw std::runtime_error("No destinations available");
  }
}

int MySQLRouting::set_destination_connect_timeout(int seconds) {
  if (seconds <= 0 || seconds > UINT16_MAX) {
    auto err = string_format("[%s] tried to set destination_connect_timeout using invalid value, was '%d'",
                             name.c_str(), seconds);
    throw std::invalid_argument(err);
  }
  destination_connect_timeout_ = seconds;
  return destination_connect_timeout_;
}

int MySQLRouting::set_max_connections(int maximum) {
  if (maximum <= 0 || maximum > UINT16_MAX) {
    auto err = string_format("[%s] tried to set max_connections using invalid value, was '%d'", name.c_str(),
                             maximum);
    throw std::invalid_argument(err);
  }
  max_connections_ = maximum;
  return max_connections_;
}
