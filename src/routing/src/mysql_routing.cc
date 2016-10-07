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
#ifdef _WIN32
#  define NOMINMAX
#endif

#include "common.h"
#include "dest_fabric_cache.h"
#include "dest_first_available.h"
#include "dest_metadata_cache.h"
#include "logger.h"
#include "mysql_routing.h"
#include "mysqlrouter/fabric_cache.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"
#include "plugin_config.h"
#include "protocol/protocol.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <sstream>

#include <sys/types.h>

#ifdef _WIN32
/* before winsock inclusion */
#  define FD_SETSIZE 4096
#else
#  undef __FD_SETSIZE
#  define __FD_SETSIZE 4096
#endif

#ifndef _WIN32
#  include <netinet/in.h>
#  include <fcntl.h>
#  include <sys/un.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#else
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

using std::runtime_error;
using std::string;
using mysql_harness::get_strerror;
using mysqlrouter::string_format;
using mysqlrouter::to_string;
using routing::AccessMode;
using mysqlrouter::URI;
using mysqlrouter::URIError;
using mysqlrouter::URIQuery;
using mysqlrouter::TCPAddress;

static const char *kDefaultReplicaSetName = "default";

MySQLRouting::MySQLRouting(routing::AccessMode mode, uint16_t port,
                           const string &protocol_name,
                           const string &bind_address,
                           const mysql_harness::Path& named_socket,
                           const string &route_name,
                           int max_connections,
                           int destination_connect_timeout,
                           unsigned long long max_connect_errors,
                           unsigned int client_connect_timeout,
                           unsigned int net_buffer_length,
                           SocketOperationsBase *socket_operations)
    : name(route_name),
      mode_(mode),
      max_connections_(set_max_connections(max_connections)),
      destination_connect_timeout_(set_destination_connect_timeout(destination_connect_timeout)),
      max_connect_errors_(max_connect_errors),
      client_connect_timeout_(client_connect_timeout),
      net_buffer_length_(net_buffer_length),
      bind_address_(TCPAddress(bind_address, port)),
      bind_named_socket_(named_socket),
      stopping_(false),
      info_active_routes_(0),
      info_handled_routes_(0),
      socket_operations_(socket_operations),
      protocol_(Protocol::create_protocol(protocol_name, socket_operations)) {

  assert(socket_operations_ != nullptr);

  #ifdef _WIN32
  if (named_socket.is_set()) {
    throw std::invalid_argument(string_format("'socket' configuration item is not supported on Windows platform"));
  }
  #endif

  // This test is only a basic assertion.  Calling code is expected to check the validity of these arguments more thoroughally.
  // At the time of writing, routing_plugin.cc : init() is one such place.
  if (!bind_address_.port && !named_socket.is_set()) {
    throw std::invalid_argument(string_format("No valid address:port (%s:%d) or socket (%s) to bind to", bind_address.c_str(), port, named_socket.c_str()));
  }
}

bool MySQLRouting::block_client_host(const std::array<uint8_t, 16> &client_ip_array,
                                     const string &client_ip_str, int server) {
  bool blocked = false;
  {
    std::lock_guard<std::mutex> lock(mutex_auth_errors_);

    if (++auth_error_counters_[client_ip_array] >= max_connect_errors_) {
      log_warning("[%s] blocking client host %s", name.c_str(), client_ip_str.c_str());
      blocked = true;
    } else {
      log_info("[%s] %d authentication errors for %s (max %u)",
               name.c_str(), auth_error_counters_[client_ip_array], client_ip_str.c_str(), max_connect_errors_);
    }
  }

  if (server >= 0) {
    protocol_->on_block_client_host(server, name);
  }

  return blocked;
}

void MySQLRouting::routing_select_thread(int client, const in6_addr client_addr) noexcept {
  int nfds;
  int res;
  int error = 0;
  size_t bytes_down = 0;
  size_t bytes_up = 0;
  size_t bytes_read = 0;
  string extra_msg = "";
  RoutingProtocolBuffer buffer(net_buffer_length_);
  bool handshake_done = false;

  int server = destination_->get_server_socket(destination_connect_timeout_, &error);

  if (!(server > 0 && client > 0)) {
    std::stringstream os;
    os << "Can't connect to remote MySQL server for client '"
      << bind_address_.addr << ":" << bind_address_.port << "'";
    log_warning("[%s] %s", name.c_str(), os.str().c_str());

    // at this point, it does not matter whether client gets the error
    protocol_->send_error(client, 2003, os.str(), "HY000", name);

    socket_operations_->shutdown(client);
    socket_operations_->shutdown(server);

    if (client > 0) {
      socket_operations_->close(client);
    }
    if (server > 0) {
      socket_operations_->close(server);
    }
    return;
  }

  std::pair<std::string, int> c_ip = get_peer_name(client);
  std::pair<std::string, int> s_ip = get_peer_name(server);

  std::string info;
  if (c_ip.second == 0) {
    // Unix socket/Windows Named pipe
    info = string_format("[%s] source %s - dest [%s]:%d",
                         name.c_str(), bind_named_socket_.c_str(),
                         s_ip.first.c_str(), s_ip.second);
  } else {
    info = string_format("[%s] source [%s]:%d - dest [%s]:%d",
                         name.c_str(), c_ip.first.c_str(), c_ip.second,
                         s_ip.first.c_str(), s_ip.second);
  }
  log_debug(info.c_str());

  ++info_handled_routes_;

  nfds = std::max(client, server) + 1;

  int pktnr = 0;
  while (true) {
    fd_set readfds;
    fd_set errfds;
    // Reset on each loop
    FD_ZERO(&readfds);
    FD_ZERO(&errfds);
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
        extra_msg = string("Select failed with error: " + get_strerror(errno));
#ifdef _WIN32
      } else if (WSAGetLastError() > 0) {
        extra_msg = string("Select failed with error: " + get_message_error(WSAGetLastError()));
#endif
      } else {
        extra_msg = string("Select failed (" + to_string(res) + ")");
      }

      break;
    }

    // Handle traffic from Server to Client
    // Note: Server _always_ talks first
    if (protocol_->copy_packets(server, client,
                                &readfds, buffer, &pktnr,
                                handshake_done, &bytes_read, true) == -1) {
#ifndef _WIN32
      if (errno > 0) {
#else
      if (errno > 0 || WSAGetLastError() != 0) {
#endif
        extra_msg = string("Copy server-client failed: " + to_string(get_message_error(errno)));
      }
      break;
    }
    bytes_up += bytes_read;

    // Handle traffic from Client to Server
    if (protocol_->copy_packets(client, server,
                                &readfds, buffer, &pktnr,
                                handshake_done, &bytes_read, false) == -1) {
      break;
    }
    bytes_down += bytes_read;

  } // while (true)

  if (!handshake_done) {
    auto ip_array = in6_addr_to_array(client_addr);
    log_debug("[%s] Routing failed for %s: %s", name.c_str(), c_ip.first.c_str(), extra_msg.c_str());
    block_client_host(ip_array, c_ip.first.c_str(), server);
  }

  // Either client or server terminated
  socket_operations_->shutdown(client);
  socket_operations_->shutdown(server);
  socket_operations_->close(client);
  socket_operations_->close(server);

  --info_active_routes_;
#ifndef _WIN32
  log_debug("[%s] Routing stopped (up:%zub;down:%zub) %s", name.c_str(), bytes_up, bytes_down, extra_msg.c_str());
#else
  log_debug("[%s] Routing stopped (up:%Iub;down:%Iub) %s", name.c_str(), bytes_up, bytes_down, extra_msg.c_str());
#endif
}

void MySQLRouting::start() {

  std::shared_ptr<void> scope_exit_guard(nullptr, [this](void*){
    if (thread_tcp_.joinable()) {
      thread_tcp_.join();
    }
    if (thread_named_socket_.joinable()) {
      thread_named_socket_.join();
    }
  });

  if (bind_address_.port > 0) {
    try {
      setup_tcp_service();
    } catch (const runtime_error &exc) {
      stop();
      throw runtime_error(
          string_format("Setting up TCP service using %s: %s", bind_address_.str().c_str(), exc.what()));
    }
    log_info("[%s] started: listening on %s; %s", name.c_str(), bind_address_.str().c_str(),
             routing::get_access_mode_name(mode_).c_str());
    thread_tcp_ = std::thread(&MySQLRouting::start_tcp_service, this);
  }

#ifndef _WIN32
  if (bind_named_socket_.is_set()) {
    try {
      setup_named_socket_service();
    } catch (const runtime_error &exc) {
      stop();
      throw runtime_error(
          string_format("Setting up named socket service '%s': %s", bind_named_socket_.c_str(), exc.what()));
    }

    log_info("[%s] started: listening using %s; %s", name.c_str(), bind_named_socket_.c_str(),
             routing::get_access_mode_name(mode_).c_str());
    thread_named_socket_ = std::thread(&MySQLRouting::start_named_socket_service, this);
  }
#endif
}

void MySQLRouting::start_tcp_service() {
  int sock_client;
  struct sockaddr_in6 client_addr;
  socklen_t sin_size = static_cast<socklen_t>(sizeof client_addr);
  char client_ip[INET6_ADDRSTRLEN];
  int opt_nodelay = 1;

  destination_->start();

  while (!stopping()) {
    if ((sock_client = accept(service_tcp_, (struct sockaddr *) &client_addr, &sin_size)) < 0) {
      log_error("[%s] Failed opening socket: %s", name.c_str(), get_message_error(errno).c_str());
      continue;
    }
    log_debug("TCP connection from %i accepted at %s", sock_client, bind_address_.str().c_str());

    if (inet_ntop(AF_INET6, &client_addr, client_ip, static_cast<socklen_t>(sizeof(client_ip))) == nullptr) {
      log_error("[%s] inet_ntop failed: %s", name.c_str(), get_message_error(errno).c_str());
      continue;
    }

    if (auth_error_counters_[in6_addr_to_array(client_addr.sin6_addr)] >= max_connect_errors_) {
      std::stringstream os;
      protocol_->send_error(sock_client, 1129, os.str(), "HY000", name);
      log_debug("%s", os.str().c_str());
      socket_operations_->close(sock_client); // no shutdown() before close()
      continue;
    }

    if (info_active_routes_.load(std::memory_order_relaxed) >= max_connections_) {
      protocol_->send_error(sock_client, 1040, "Too many connections", "HY000", name);
      socket_operations_->close(sock_client); // no shutdown() before close()
      log_warning("[%s] reached max active connections (%d)", name.c_str(), max_connections_);
      continue;
    }

    if (setsockopt(sock_client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&opt_nodelay), static_cast<socklen_t>(sizeof(int))) == -1) {
      log_error("[%s] client setsockopt error: %s", name.c_str(), get_message_error(errno).c_str());
      continue;
    }

    ++info_active_routes_;
    std::thread(&MySQLRouting::routing_select_thread, this, sock_client, client_addr.sin6_addr).detach();
  } // while (!stopping())

  log_info("[%s] stopped", name.c_str());
}

#ifndef _WIN32
void MySQLRouting::start_named_socket_service() {
  struct sockaddr_in6 client_addr;
  socklen_t sin_size = static_cast<socklen_t>(sizeof client_addr);

  int sock_client;

  while (!stopping()) {
    if (errno > 0) {
      log_error(get_strerror(errno).c_str());
      errno = 0;
    }

    if ((sock_client = accept(service_named_socket_, (struct sockaddr *) &client_addr, &sin_size)) < 0) {
      continue;
    }

    if (info_active_routes_.load(std::memory_order_relaxed) >= routing::kDefaultMaxConnections) {
      shutdown(sock_client, SHUT_RDWR);
      log_warning("%s reached max active connections (%d)", name.c_str(), routing::kDefaultMaxConnections);
      continue;
    }

    std::thread(&MySQLRouting::routing_select_thread, this, sock_client, client_addr.sin6_addr).detach();
  }
}
#endif

void MySQLRouting::stop() {
  stopping_.store(true);
}

void MySQLRouting::setup_tcp_service() {
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
    if ((service_tcp_ = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
      throw std::runtime_error(get_strerror(errno));
    }

#ifndef _WIN32
    option_value = 1;
    if (setsockopt(service_tcp_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&option_value),
            static_cast<socklen_t>(sizeof(int))) == -1) {
      throw std::runtime_error(get_message_error(errno));
    }
#endif

    if (::bind(service_tcp_, info->ai_addr, info->ai_addrlen) == -1) {
      socket_operations_->close(service_tcp_);
#ifdef _WIN32
      int errcode = WSAGetLastError();
#else
      int errcode = errno;
#endif
      throw std::runtime_error(get_message_error(errcode));
    }
    break;
  }
  freeaddrinfo(servinfo);

  if (info == nullptr) {
    throw runtime_error(string_format("[%s] Failed to setup server socket", name.c_str()));
  }

  if (listen(service_tcp_, 20) < 0) {
    throw runtime_error(string_format("[%s] Failed to start listening for connections using TCP", name.c_str()));
  }
}

#ifndef _WIN32
void MySQLRouting::setup_named_socket_service() {
  struct sockaddr_un sock_unix;
  string socket_file = bind_named_socket_.str();
  errno = 0;

  assert(!socket_file.empty());
  assert(socket_file.size() < (sizeof(sockaddr_un().sun_path)-1)); // We already did check reading the configuration

  // Try removing any socket previously created
  if (unlink(socket_file.c_str()) == -1) {
    if (errno != 2) {
      throw std::runtime_error(
          "Failed removing socket file " + socket_file + " (" + get_strerror(errno) + " (" + to_string(errno) + "))");
    }
    errno = 0;
  }

  if ((service_named_socket_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    throw std::runtime_error(get_strerror(errno));
  }

  sock_unix.sun_family = AF_UNIX;
  std::strncpy(sock_unix.sun_path, socket_file.c_str(), socket_file.size() + 1);

  if (::bind(service_named_socket_, (struct sockaddr *) &sock_unix, static_cast<socklen_t>(sizeof(sock_unix))) == -1) {
    throw std::runtime_error(get_strerror(errno));
  }

  if (listen(service_named_socket_, 20) < 0) {
    throw runtime_error("Failed to start listening for connections using named socket");
  }
}
#endif

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
  } else if (uri.scheme == "metadata-cache") {
    // Syntax: metadata_cache://[<metadata_cache_config(unused)>]/<replicaset_name>?role=PRIMARY|SECONDARY
    std::string replicaset_name = kDefaultReplicaSetName;
    std::string role;

    if (uri.path.size() > 0 && !uri.path[0].empty())
      replicaset_name = uri.path[0];
    if (uri.query.find("role") == uri.query.end())
      throw runtime_error("Missing 'role' in routing destination specification");

    destination_.reset(new DestMetadataCacheGroup(uri.host, replicaset_name,
                                                  get_access_mode_name(mode_),
                                                  uri.query, protocol_->get_name()));
  } else {
    throw runtime_error(string_format("Invalid URI scheme '%s' for URI %s",
                                      uri.scheme.c_str()));
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
