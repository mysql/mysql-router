/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "destfailover.h"
#include "dest_fabric_cache.h"
#include "uri.h"
#include "plugin_config.h"
#include "mysqlrouter/routing.h"

#include <cstring>
#include <future>
#include <sstream>
#include <sys/fcntl.h>
#include <string>
#include <memory>

#include "mysqlrouter/utils.h"
#include "logger.h"

using std::cout;
using std::cerr;
using std::endl;
using std::runtime_error;
using mysqlrouter::string_format;
using mysqlrouter::to_string;
using routing::AccessMode;

MySQLRouting::MySQLRouting(routing::AccessMode mode, uint16_t port, const string &bind_address,
                           const string &route_name)
    : name(route_name),
      mode_(mode),
      wait_timeout_(routing::kDefaultWaitTimeout),
      destination_connect_timeout_(routing::kDefaultDestinationConnectionTimeout),
      bind_address_(TCPAddress(bind_address, port)),
      stopping_(false),
      info_active_routes_(0),
      info_handled_routes_(0) { }

MySQLRouting::MySQLRouting(const RoutingPluginConfig &config)
    : name(config.section_name),
      mode_(config.mode),
      wait_timeout_(config.wait_timeout),
      destination_connect_timeout_(routing::kDefaultDestinationConnectionTimeout),
      bind_address_(config.bind_address),
      stopping_(false),
      info_active_routes_(0),
      info_handled_routes_(0) { }


/** @brief Reads from sender and writes it back to receiver using select
 *
 * This function reads data from the sender socket and writes it back
 * to the receiver socket. It use `select`.
 *
 * @param buffer Buffer to use for reading and writing back
 * @param sender Descriptor of the sender
 * @param receiver Descriptor of the receiver
 * @param readfds Read descriptors used with FD_ISSET
 * @return Bytes read or -1 on error
 */
ssize_t copy_mysql_protocol_packets(int sender, int receiver, fd_set *readfds) {
  char buffer[UINT16_MAX];
  ssize_t bytes = 0;

  if (FD_ISSET(sender, readfds)) {
    if ((bytes = read(sender, buffer, sizeof(buffer))) <= 0) {
      return -1;
    }
    ssize_t total_written = 0;
    ssize_t written = 0;
    while (total_written != bytes) {
      if ((written = write(receiver, buffer, static_cast<size_t >(bytes))) == -1) {
        return -1;
      }
      total_written += written;
    }
  }

  return bytes;
}

int MySQLRouting::get_destination() noexcept {

  if (destination_->empty()) {
    return 0;
  }

  destination_->rewind();
  for (size_t i = 0; i < destination_->size(); ++i) {
    auto addr = destination_->get_server();  // Validated when added
    auto s = get_mysql_connection(addr);
    if (s) {
      return s;
    }
  }
  return 0;
}

void MySQLRouting::thd_routing_select(int client) noexcept {
  ssize_t bytes = 0;
  int nfds;
  int res;
  struct timeval timeout_val;
  size_t bytes_down = 0;
  size_t bytes_up = 0;
  string extra_msg;

  int server = get_destination();

  if (not (server > 0 && client > 0)) {
    shutdown(client, SHUT_RDWR);
    shutdown(server, SHUT_RDWR);
    return;
  }

  auto c_ip = get_peer_name(client);
  auto s_ip = get_peer_name(server);

  std::string info = string_format("%s [%s]:%d - [%s]:%d", name.c_str(), c_ip.first.c_str(), c_ip.second,
                                   s_ip.first.c_str(), s_ip.second);
  log_info(info.c_str());
  ++info_handled_routes_;
  ++info_active_routes_;

  nfds = ((client > server) ? client : server) + 1;

  while (true) {
    fd_set readfds;
    // Reset on each loop
    FD_ZERO(&readfds);
    FD_SET(client, &readfds);
    FD_SET(server, &readfds);
    timeout_val.tv_sec = wait_timeout_;
    timeout_val.tv_usec = 0;

    if ((res = select(nfds, &readfds, nullptr, nullptr, &timeout_val)) <= 0) {
      if (res == 0) {
        extra_msg = string("Wait timeout reached (" + to_string(wait_timeout_) + ")");
      } else if (errno > 0) {
        extra_msg = string("Select failed with error: " + to_string(strerror(errno)));
      }

      break;
    }

    // Handle traffic from Server to Client
    // Note: Server _always_ always talks first
    if ((bytes = copy_mysql_protocol_packets(server, client, &readfds)) == -1) {
      break;
    }
    if (bytes_up < SIZE_MAX) {
      bytes_up += static_cast<size_t >(bytes);
    } else {
      bytes_up = static_cast<size_t >(bytes);
      log_debug("Bytes upstream reset");
    }

    // Handle traffic from Client to Server
    if ((bytes = copy_mysql_protocol_packets(client, server, &readfds)) == -1) {
      break;
    }
    if (bytes_down < SIZE_MAX) {
      bytes_down += static_cast<size_t >(bytes);
    } else {
      bytes_down = static_cast<size_t >(bytes);
      log_info("Bytes downstream reset");
    }
  }

  // Either client or server terminated
  shutdown(client, SHUT_RDWR);
  shutdown(server, SHUT_RDWR);
  // Using more portable stringstream instead of formatting size_t
  --info_active_routes_;
  std::ostringstream os;
  os << "Routing stopped (up:" << bytes_up << "b;down:" << bytes_down << "b" << ") " << extra_msg;
  log_debug(os.str().c_str());
}

void MySQLRouting::start() {
  int sock_client;
  struct sockaddr_in6 client_addr;
  socklen_t sin_size = sizeof client_addr;
  char client_ip[INET6_ADDRSTRLEN];
  int opt_nodelay = 0;

  try {
    setup_service();
  } catch (const runtime_error &exc) {
    throw runtime_error(
        string_format("Setting up service using %s: %s", bind_address_.str().c_str(), exc.what()));
  }

  log_info("%s started: listening on %s; %s", name.c_str(), bind_address_.str().c_str(),
           routing::get_access_mode_name(mode_).c_str());

#ifndef ENABLE_TESTS
  while (!stopping()) {
    if (errno > 0) {
      log_error(strerror(errno));
      errno = 0;
    }

    if ((sock_client = accept(sock_server_, (struct sockaddr *) &client_addr, &sin_size)) < 0) {
      continue;
    }

    if (info_active_routes_.load(std::memory_order_relaxed) >= routing::kDefaultMaxConnections) {
      shutdown(sock_client, SHUT_RDWR);
      log_warning("%s reached max active connections (%d)", name.c_str(), routing::kDefaultMaxConnections);
      continue;
    }

    if (setsockopt(sock_client, IPPROTO_TCP, TCP_NODELAY, &opt_nodelay, sizeof(int)) == -1) {
      continue;
    }

    if (inet_ntop(AF_INET6, &client_addr, client_ip, sizeof client_ip) == nullptr) {
      continue;
    }

    std::thread(&MySQLRouting::thd_routing_select, this, sock_client).detach();
  }
#endif

  log_info("%s stopped", name.c_str());
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
    throw runtime_error(string_format("Failed getting address information (%s)", gai_strerror(err)));
  }

  // Try to setup socket and bind
  for (info = servinfo; info != nullptr; info = info->ai_next) {
    if (errno > 0) {
      throw std::runtime_error(strerror(errno));
    }

    if ((sock_server_ = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
      continue;
    }

    option_value = 1;
    if (setsockopt(sock_server_, SOL_SOCKET, SO_REUSEADDR, &option_value, sizeof(int)) == -1) {
      continue;
    }

    if (::bind(sock_server_, info->ai_addr, info->ai_addrlen) == -1) {
      close(sock_server_);
      continue;
    }
    break;
  }
  freeaddrinfo(servinfo);

  if (info == nullptr) {
    throw runtime_error("Failed to setup server socket");
  }

  if (listen(sock_server_, 20) < 0) {
    throw runtime_error("Failed to start listening for connections");
  }
}

void MySQLRouting::set_destinations_from_uri(const URI &uri) {
  if (uri.scheme == "fabric+cache") {
    destination_.reset(new DestFabricCacheGroup(uri.host, uri.path[1], mode_, uri.query));
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
    destination_.reset(new DestFailover());
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

int MySQLRouting::get_mysql_connection(mysqlrouter::TCPAddress addr) noexcept {
  fd_set readfds;
  struct timeval timeout_val;

  struct addrinfo *servinfo, *info, hints;
  int err;
  int res, connect_res;
  int sock = 0;
  int so_error = 0;
  socklen_t error_len = sizeof(so_error);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  errno = 0;

  if ((err = getaddrinfo(addr.addr.c_str(), to_string(addr.port).c_str(), &hints, &servinfo)) != 0) {
    log_debug("Failed getting address information for '%s' (%s)", addr.addr.c_str(), gai_strerror(err));
    return 0;
  }

  for (info = servinfo; info != nullptr; info = info->ai_next) {
    // Handle errors from previous try
    if ((errno > 0 && errno != EINPROGRESS) || so_error) {
      break;
    }

    if ((sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
      continue;
    }
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    timeout_val.tv_sec = destination_connect_timeout_;
    timeout_val.tv_usec = 0;

    // Set non-blocking so we can timeout using select()
    set_socket_blocking(sock, false);
    connect_res = connect(sock, info->ai_addr, info->ai_addrlen);
    if (connect_res == -1 && errno != EINPROGRESS) {
      continue;
    }

    res = select(sock + 1, &readfds, nullptr, nullptr, &timeout_val);
    if (res <= 0) {
      if (res == 0) {
        shutdown(sock, SHUT_RDWR);
        log_debug("Timeout reached connecting with MySQL Server %s", addr.str().c_str());
        return 0;
      }
      continue;
    }

    getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &error_len);
    if (FD_ISSET(sock, &readfds) && !so_error) {
      set_socket_blocking(sock, false);
      errno = 0;
      break;
    }
  }

  // Handle remaining errors
  if ((errno > 0 && errno != EINPROGRESS) || so_error) {
    shutdown(sock, SHUT_RDWR);
    auto errmsg = string_format("Destination %s %%s (%%d)", addr.str().c_str());
    if (so_error) {
      log_debug(errmsg.c_str(), strerror(so_error), so_error);
    } else {
      log_debug(errmsg.c_str(), strerror(errno), errno);
    }
    return 0;
  }

  return sock;
}

