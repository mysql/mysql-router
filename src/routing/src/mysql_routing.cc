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
#ifdef _WIN32
#  define NOMINMAX
#endif

#include "common.h"
#include "dest_first_available.h"
#include "dest_next_available.h"
#include "dest_round_robin.h"
#include "dest_metadata_cache.h"
#include "mysql/harness/logging/logging.h"
#include "mysql_routing.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"
#include "mysql/harness/plugin.h"
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

#if defined(__sun)
# include <ucred.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
# include <sys/ucred.h>
#endif

using std::runtime_error;
using std::string;
using mysql_harness::get_strerror;
using mysqlrouter::string_format;
using mysqlrouter::to_string;
using routing::AccessMode;
using routing::RoutingStrategy;
using mysqlrouter::URI;
using mysqlrouter::URIError;
using mysqlrouter::URIQuery;
using mysqlrouter::TCPAddress;
using mysqlrouter::is_valid_socket_name;
IMPORT_LOG_FUNCTIONS()

static int kListenQueueSize = 1024;

static const char *kDefaultReplicaSetName = "default";
static const std::chrono::milliseconds kAcceptorStopPollInterval_ms { 100 };

MySQLRouting::MySQLRouting(routing::RoutingStrategy routing_strategy, uint16_t port,
                           const Protocol::Type protocol,
                           const routing::AccessMode access_mode,
                           const string &bind_address,
                           const mysql_harness::Path& named_socket,
                           const string &route_name,
                           int max_connections,
                           std::chrono::milliseconds destination_connect_timeout,
                           unsigned long long max_connect_errors,
                           std::chrono::milliseconds client_connect_timeout,
                           unsigned int net_buffer_length,
                           SocketOperationsBase *socket_operations)
    : name(route_name),
      routing_strategy_(routing_strategy),
      access_mode_(access_mode),
      max_connections_(set_max_connections(max_connections)),
      destination_connect_timeout_(set_destination_connect_timeout(destination_connect_timeout)),
      max_connect_errors_(max_connect_errors),
      client_connect_timeout_(client_connect_timeout),
      net_buffer_length_(net_buffer_length),
      bind_address_(TCPAddress(bind_address, port)),
      bind_named_socket_(named_socket),
      service_tcp_(routing::kInvalidSocket),
      service_named_socket_(routing::kInvalidSocket),
      info_active_routes_(0),
      info_handled_routes_(0),
      socket_operations_(socket_operations),
      protocol_(Protocol::create(protocol, socket_operations)) {

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

MySQLRouting::~MySQLRouting() {

  if (service_tcp_ != routing::kInvalidSocket) {
    socket_operations_->shutdown(service_tcp_);
    socket_operations_->close(service_tcp_);
  }
}

bool MySQLRouting::block_client_host(const std::array<uint8_t, 16> &client_ip_array,
                                     const string &client_ip_str, int server) {
  bool blocked = false;
  {
    std::lock_guard<std::mutex> lock(mutex_conn_errors_);

    if (++conn_error_counters_[client_ip_array] >= max_connect_errors_) {
      log_warning("[%s] blocking client host %s", name.c_str(), client_ip_str.c_str());
      blocked = true;
    } else {
      log_info("[%s] %lu connection errors for %s (max %llu)", name.c_str(),
               static_cast<unsigned long>(conn_error_counters_[client_ip_array]), // 32bit Linux requires cast
               client_ip_str.c_str(), max_connect_errors_);
    }
  }

  if (server >= 0) {
    protocol_->on_block_client_host(server, name);
  }

  return blocked;
}

const std::vector<std::array<uint8_t, 16>> MySQLRouting::get_blocked_client_hosts() const {
  std::lock_guard<std::mutex> lock(mutex_conn_errors_);

  std::vector<std::array<uint8_t, 16>> result;
  for(const auto& client_ip: conn_error_counters_) {
    if (client_ip.second >= max_connect_errors_) {
      result.push_back(client_ip.first);
    }
  }

  return result;
}

/*static*/
std::string MySQLRouting::make_thread_name(const std::string& config_name, const std::string& prefix) {

  const char* p = config_name.c_str();

  // at the time of writing, config_name starts with:
  //   "routing:<config_from_conf_file>" (with key)
  // or with:
  //   "routing" (without key).
  // Verify this assumption
  constexpr char kRouting[] = "routing";
  size_t kRoutingLen = sizeof(kRouting) - 1;  // -1 to ignore string terminator
  if (memcmp(p, kRouting, kRoutingLen))
    return prefix + ":parse err";

  // skip over "routing[:]"
  p += kRoutingLen;
  if (*p == ':')
    p++;

  // at the time of writing, bootstrap generates 4 routing configurations by default,
  // which will result in <config_from_conf_file> having one of below 4 values:
  //   "<cluster_name>_default_ro",   "<cluster_name>_default_rw",
  //   "<cluster_name>_default_x_ro", "<cluster_name>_default_x_rw"
  // since we're limited to 15 chars for thread name, we skip over
  // "<cluster_name>_default_" so that suffixes ("x_ro", etc) can fit
  std::string key = p;
  const char kPrefix[] = "_default_";
  if (key.find(kPrefix) != key.npos) {
    key = key.substr(key.find(kPrefix) + sizeof(kPrefix) - 1);  // -1 for string terminator
  }

  // now put everything together
  std::string thread_name = prefix + ":" + key;
  thread_name.resize(15); // max for pthread_setname_np()

  return thread_name;
}

void MySQLRouting::routing_select_thread(mysql_harness::PluginFuncEnv* env,
                                         int client,
                                         const sockaddr_storage& /* client_addr */) noexcept {
  mysql_harness::rename_thread(make_thread_name(name, "RtC").c_str());  // "Rt client thread" would be too long :(
  {
    std::lock_guard<std::mutex> lk(active_client_threads_cond_m_);
    active_client_threads_++;
  }
  active_client_threads_cond_.notify_all();

  std::shared_ptr<void> exit_guard_active_threads(nullptr, [&](void *){
      std::lock_guard<std::mutex> lk(active_client_threads_cond_m_);
      active_client_threads_--;

      // notify the parent while we have the cond_mutex locked
      // otherwise the parent may destruct before we are finished.
      active_client_threads_cond_.notify_all();
  });

  int error = 0;
  size_t bytes_down = 0;
  size_t bytes_up = 0;
  size_t bytes_read = 0;
  string extra_msg = "";
  RoutingProtocolBuffer buffer(net_buffer_length_);
  bool handshake_done = false;

  int server = destination_->get_server_socket(destination_connect_timeout_, &error);

  if ((server == routing::kInvalidSocket) ||
      (client == routing::kInvalidSocket)) {
    std::stringstream os;
    os << "Can't connect to remote MySQL server for client connected to '"
      << bind_address_.addr << ":" << bind_address_.port << "'";

    log_warning("[%s] fd=%d %s", name.c_str(), client, os.str().c_str());

    // at this point, it does not matter whether client gets the error
    protocol_->send_error(client, 2003, os.str(), "HY000", name);

    if (client != routing::kInvalidSocket) socket_operations_->shutdown(client);
    if (server != routing::kInvalidSocket) socket_operations_->shutdown(server);

    if (client != routing::kInvalidSocket) {
      socket_operations_->close(client);
    }
    if (server != routing::kInvalidSocket) {
      socket_operations_->close(server);
    }
    return;
  }

  std::pair<std::string, int> c_ip = get_peer_name(client);
  std::pair<std::string, int> s_ip = get_peer_name(server);

  if (c_ip.second == 0) {
    // Unix socket/Windows Named pipe
    log_debug("[%s] fd=%d connected %s -> %s:%d as fd=%d",
        name.c_str(),
        client,
        bind_named_socket_.c_str(),
        s_ip.first.c_str(), s_ip.second,
        server);
  } else {
    log_debug("[%s] fd=%d connected %s:%d -> %s:%d as fd=%d",
        name.c_str(),
        client,
        c_ip.first.c_str(), c_ip.second,
        s_ip.first.c_str(), s_ip.second,
        server);
  }

  ++info_active_routes_;
  ++info_handled_routes_;

  int pktnr = 0;

  bool connection_is_ok = true;
  while (connection_is_ok && is_running(env)) {
    const size_t kClientEventIndex = 0;
    const size_t kServerEventIndex = 1;

    struct pollfd fds[] = {
      { routing::kInvalidSocket, POLLIN, 0 },
      { routing::kInvalidSocket, POLLIN, 0 },
    };

    fds[kClientEventIndex].fd = client;
    fds[kServerEventIndex].fd = server;

    const std::chrono::milliseconds poll_timeout_ms = handshake_done ? std::chrono::milliseconds(1000) : client_connect_timeout_;
    int res = socket_operations_->poll(fds, sizeof(fds) / sizeof(fds[0]), poll_timeout_ms);

    if (res < 0) {
      const int last_errno = socket_operations_->get_errno();
      switch (last_errno) {
        case EINTR:
        case EAGAIN:
          // got interrupted. Just retry
          break;
        default:
          // break the loop, something ugly happened
          connection_is_ok = false;
          extra_msg = string("poll() failed: " + to_string(get_message_error(last_errno)));
          break;
      }

      continue;
    } else if (res == 0) {
      // timeout
      if (!handshake_done) {
        connection_is_ok = false;
        extra_msg = string("client auth timed out");

        break;
      } else {
        continue;
      }
    }

    // something happened on the socket: either we have data or the socket was closed.
    //
    // closed sockets are signalled in two ways:
    //
    // * Linux: POLLIN + read() == 0
    // * Windows: POLLHUP

    const bool client_is_readable = (fds[kClientEventIndex].revents & (POLLIN|POLLHUP)) != 0;
    const bool server_is_readable = (fds[kServerEventIndex].revents & (POLLIN|POLLHUP)) != 0;

    // Handle traffic from Server to Client
    // Note: In classic protocol Server _always_ talks first
    if (protocol_->copy_packets(server, client, server_is_readable,
                                buffer, &pktnr,
                                handshake_done, &bytes_read, true) == -1) {
      const int last_errno = socket_operations_->get_errno();
      if (last_errno > 0) {
        // if read() against closed socket, errno will be 0. Don't log that.
        extra_msg = string("Copy server->client failed: " + to_string(get_message_error(last_errno)));
      }

      connection_is_ok = false;
    } else {
      bytes_up += bytes_read;
    }

    // Handle traffic from Client to Server
    if (protocol_->copy_packets(client, server, client_is_readable,
                                buffer, &pktnr,
                                handshake_done, &bytes_read, false) == -1) {
      const int last_errno = socket_operations_->get_errno();
      if (last_errno > 0) {
        extra_msg = string("Copy client->server failed: " + to_string(get_message_error(last_errno)));
      } else if (!handshake_done) {
        extra_msg = string("Copy client->server failed: unexpected connection close");
      }
      // client close on us.
      connection_is_ok = false;
    } else {
      bytes_down += bytes_read;
    }

  } // while (connection_is_ok && is_running(env))

  if (!handshake_done) {
    log_info("[%s] fd=%d Pre-auth socket failure %s: %s",
        name.c_str(),
        client,
        c_ip.first.c_str(), extra_msg.c_str());
    // auto ip_array = in_addr_to_array(client_addr);
    // block_client_host(ip_array, c_ip.first.c_str(), server);
  }

  // Either client or server terminated
  socket_operations_->shutdown(client);
  socket_operations_->shutdown(server);
  socket_operations_->close(client);
  socket_operations_->close(server);

  --info_active_routes_;
#ifndef _WIN32
  log_debug("[%s] fd=%d connection closed (up: %zub; down: %zub) %s",
      name.c_str(),
      client, bytes_up, bytes_down, extra_msg.c_str());
#else
  log_debug("[%s] fd=%d connection closed (up: %Iub; down: %Iub) %s",
      name.c_str(),
      client, bytes_up, bytes_down, extra_msg.c_str());
#endif
}

void MySQLRouting::start(mysql_harness::PluginFuncEnv* env) {

  mysql_harness::rename_thread(make_thread_name(name, "RtM").c_str());  // "Rt main" would be too long :(
  if (bind_address_.port > 0) {
    try {
      setup_tcp_service();
    } catch (const runtime_error &exc) {
      clear_running(env);
      throw runtime_error(
          string_format("Setting up TCP service using %s: %s", bind_address_.str().c_str(), exc.what()));
    }
    log_info("[%s] started: listening on %s", name.c_str(), bind_address_.str().c_str());
  }
#ifndef _WIN32
  if (bind_named_socket_.is_set()) {
    try {
      setup_named_socket_service();
    } catch (const runtime_error &exc) {
      clear_running(env);
      throw runtime_error(
          string_format("Setting up named socket service '%s': %s", bind_named_socket_.c_str(), exc.what()));
    }
    log_info("[%s] started: listening using %s", name.c_str(), bind_named_socket_.c_str());
  }
#endif
  if (bind_address_.port > 0 || bind_named_socket_.is_set()) {
    start_acceptor(env);
#ifndef _WIN32
    if (bind_named_socket_.is_set() && unlink(bind_named_socket_.str().c_str()) == -1) {
      if (errno != ENOENT)
        log_warning("%s", ("Failed removing socket file " + bind_named_socket_.str() + " (" + get_strerror(errno) + " (" + to_string(errno) + "))").c_str());
    }
#endif
  }
}

#if !defined(_WIN32)
/*
 * get PID and UID of the other end of the unix-socket
 */
static int unix_getpeercred(int sock, pid_t &peer_pid, uid_t &peer_uid) {
#if defined(_GNU_SOURCE)
  struct ucred ucred;
  socklen_t ucred_len = sizeof(ucred);

  if (getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &ucred, &ucred_len) == -1) {
    return -1;
  }

  peer_pid = ucred.pid;
  peer_uid = ucred.uid;

  return 0;
#elif defined(__sun)
  ucred_t *ucred;

  if (getpeerucred(sock, &ucred) == -1) {
    return -1;
  }

  peer_pid = ucred_getpid(ucred);
  peer_uid = ucred_getruid(ucred);

  free(ucred);

  return 0;
#else
  // tag them as UNUSED to keep -Werror happy
  (void)(sock);
  (void)(peer_pid);
  (void)(peer_uid);

  return -1;
#endif
}
#endif

void MySQLRouting::start_acceptor(mysql_harness::PluginFuncEnv* env) {
  mysql_harness::rename_thread(make_thread_name(name, "RtA").c_str());  // "Rt Acceptor" would be too long :(

  destination_->start();

  if (service_tcp_ != routing::kInvalidSocket) {
    routing::set_socket_blocking(service_tcp_, false);
  }
  if (service_named_socket_ != routing::kInvalidSocket) {
    routing::set_socket_blocking(service_named_socket_, false);
  }

  const int kAcceptUnixSocketNdx = 0;
  const int kAcceptTcpNdx = 1;
  struct pollfd fds[] = {
    { routing::kInvalidSocket, POLLIN, 0 },
    { routing::kInvalidSocket, POLLIN, 0 },
  };

  fds[kAcceptTcpNdx].fd = service_tcp_;
  fds[kAcceptUnixSocketNdx].fd = service_named_socket_;

  while (is_running(env)) {
    // wait for the accept() sockets to become readable (POLLIN)

    int ready_fdnum = socket_operations_->poll(fds, sizeof(fds) / sizeof(fds[0]), kAcceptorStopPollInterval_ms);
    // < 0 - failure
    // == 0 - timeout
    // > 0  - number of pollfd's with a .revent

    if (ready_fdnum < 0) {
      const int last_errno = socket_operations_->get_errno();
      switch (last_errno) {
        case EINTR:
        case EAGAIN:
          continue;
        default:
          log_error("[%s] poll() failed with error: %s", name.c_str(), get_message_error(last_errno).c_str());
          break;
      }
    }

    for (size_t ndx = 0; ndx < sizeof(fds) / sizeof(fds[0]) && ready_fdnum > 0; ndx++) {
      // walk through all fields and check which fired

      if ((fds[ndx].revents & POLLIN) == 0) {
        continue;
      }

      --ready_fdnum;

      int sock_client;
      struct sockaddr_storage client_addr;
      socklen_t sin_size = static_cast<socklen_t>(sizeof client_addr);


      if ((sock_client = accept(fds[ndx].fd, (struct sockaddr *) &client_addr, &sin_size)) < 0) {
        log_error("[%s] Failed accepting connection: %s", name.c_str(), get_message_error(socket_operations_->get_errno()).c_str());
        continue;
      }

      bool is_tcp = (ndx == kAcceptTcpNdx);

      if (is_tcp) {
        log_debug("[%s] fd=%d connection accepted at %s", name.c_str(), sock_client, bind_address_.str().c_str());
      } else {
#if !defined(_WIN32)
        pid_t peer_pid;
        uid_t peer_uid;

        // try to be helpful of who tried to connect to use and failed.
        // who == PID + UID
        //
        // if we can't get the PID, we'll just show a simpler errormsg

        if (0 == unix_getpeercred(sock_client, peer_pid, peer_uid)) {
          log_debug("[%s] fd=%d connection accepted at %s from (pid=%d, uid=%d)",
              name.c_str(), sock_client, bind_named_socket_.str().c_str(),
              peer_pid, peer_uid);
        } else
          // fall through
#endif
        log_debug("[%s] fd=%d connection accepted at %s",
            name.c_str(), sock_client, bind_named_socket_.str().c_str());
      }

      if (conn_error_counters_[in_addr_to_array(client_addr)] >= max_connect_errors_) {
        std::stringstream os;
        os << "Too many connection errors from " << get_peer_name(sock_client).first;
        protocol_->send_error(sock_client, 1129, os.str(), "HY000", name);
        log_info("%s", os.str().c_str());
        socket_operations_->close(sock_client); // no shutdown() before close()
        continue;
      }

      if (info_active_routes_.load(std::memory_order_relaxed) >= max_connections_) {
        protocol_->send_error(sock_client, 1040, "Too many connections to MySQL Router", "HY000", name);
        socket_operations_->close(sock_client); // no shutdown() before close()
        log_warning("[%s] reached max active connections (%d max=%d)", name.c_str(),
                   info_active_routes_.load(), max_connections_);
        continue;
      }

      int opt_nodelay = 1;
      if (is_tcp && setsockopt(sock_client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&opt_nodelay), static_cast<socklen_t>(sizeof(int))) == -1) {
        log_info("[%s] fd=%d client setsockopt(TCP_NODELAY) failed: %s", name.c_str(), sock_client, get_message_error(socket_operations_->get_errno()).c_str());

        // if it fails, it will be slower, but cause no harm
      }

      // On some OS'es the socket will be non-blocking as a result of accept()
      // on non-blocking socket. We need to make sure it's always blocking.
      routing::set_socket_blocking(sock_client, true);

      // launch client thread which will service this new connection
      {
        auto thread_spawn_failure_handler = [&](const std::system_error* exc) {
          protocol_->send_error(sock_client, 1040,
                                "Router couldn't spawn a new thread to service new client connection",
                                "HY000", name);
          socket_operations_->close(sock_client); // no shutdown() before close()

          // we only want to log this message once, because in a low-resource situation, this would
          // lead do a DoS against ourselves (heavy I/O and disk full)
          static bool logged_this_before = false;
          if (logged_this_before)
            return;

          logged_this_before = true;
          if (exc)
            log_error("Couldn't spawn a new thread to service new client connection from %s: %s."
                      " This message will not be logged again until Router restarts.",
                      get_peer_name(sock_client).first.c_str(), exc->what());
          else
            log_error("Couldn't spawn a new thread to service new client connection from %s."
                      " This message will not be logged again until Router restarts.",
                      get_peer_name(sock_client).first.c_str());
        };

        try {
          std::thread(&MySQLRouting::routing_select_thread, this, env, sock_client, client_addr).detach();
        } catch (const std::system_error& e) {
          thread_spawn_failure_handler(&e);
          continue;
        } catch (...) {
          // According to http://www.cplusplus.com/reference/thread/thread/thread/,
          // depending on the library implementation, std::thread constructor may also throw other
          // exceptions, such as bad_alloc or system_error with different a condition.
          // Thus we have this catch(...) here to take care of the rest of them in a generic way.
          thread_spawn_failure_handler(nullptr);
          continue;
        }
      }

    }
  } // while (is_running(env))

  {
    std::unique_lock<std::mutex> lk(active_client_threads_cond_m_);
    active_client_threads_cond_.wait(lk, [&]{ return active_client_threads_ == 0;});
  }

  log_info("[%s] stopped", name.c_str());
}

static int get_socket_errno() {
#ifdef _WIN32
  return GetLastError();
#else
  return errno;
#endif
}

void MySQLRouting::setup_tcp_service() {
  struct addrinfo *servinfo, *info, hints;
  int err;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  errno = 0;

  err = socket_operations_->getaddrinfo(bind_address_.addr.c_str(),
                                        to_string(bind_address_.port).c_str(), &hints, &servinfo);
  if (err != 0) {
    throw runtime_error(string_format("[%s] Failed getting address information (%s)",
                                      name.c_str(), gai_strerror(err)));
  }

  std::shared_ptr<void> exit_guard(nullptr, [&](void*){if (servinfo) socket_operations_->freeaddrinfo(servinfo);});

  // Try to setup socket and bind
  std::string error;
  for (info = servinfo; info != nullptr; info = info->ai_next) {
    if ((service_tcp_ = socket_operations_->socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
      error = get_message_error(get_socket_errno());
      log_warning("[%s] setup_tcp_service() error from socket(): %s", name.c_str(), error.c_str());
      continue;
    }

#if 1
    int option_value = 1;
    if (socket_operations_->setsockopt(service_tcp_, SOL_SOCKET, SO_REUSEADDR, &option_value,
            static_cast<socklen_t>(sizeof(int))) == -1) {
      error = get_message_error(get_socket_errno());
      log_warning("[%s] setup_tcp_service() error from setsockopt(): %s", name.c_str(), error.c_str());
      socket_operations_->close(service_tcp_);
      service_tcp_ = routing::kInvalidSocket;
      continue;
    }
#endif

    if (socket_operations_->bind(service_tcp_, info->ai_addr, info->ai_addrlen) == -1) {
      error = get_message_error(get_socket_errno());
      log_warning("[%s] setup_tcp_service() error from bind(): %s", name.c_str(), error.c_str());
      socket_operations_->close(service_tcp_);
      service_tcp_ = routing::kInvalidSocket;
      continue;
    }

    break;
  }

  if (info == nullptr) {
    throw runtime_error(string_format("[%s] Failed to setup service socket: %s", name.c_str(), error.c_str()));
  }

  if (socket_operations_->listen(service_tcp_, kListenQueueSize) < 0) {
    throw runtime_error(string_format("[%s] Failed to start listening for connections using TCP", name.c_str()));
  }
}

#ifndef _WIN32
void MySQLRouting::setup_named_socket_service() {
  struct sockaddr_un sock_unix;
  string socket_file = bind_named_socket_.str();
  errno = 0;

  assert(!socket_file.empty());

  std::string error_msg;
  if (!is_valid_socket_name(socket_file, error_msg)) {
    throw std::runtime_error(error_msg);
  }

  if ((service_named_socket_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    throw std::invalid_argument(get_strerror(errno));
  }

  sock_unix.sun_family = AF_UNIX;
  std::strncpy(sock_unix.sun_path, socket_file.c_str(), socket_file.size() + 1);

retry:
  if (::bind(service_named_socket_, (struct sockaddr *) &sock_unix, static_cast<socklen_t>(sizeof(sock_unix))) == -1) {
    int save_errno = errno;
    if (errno == EADDRINUSE) {
      // file exists, try to connect to it to see if the socket is already in use
      if (::connect(service_named_socket_,
                    (struct sockaddr *) &sock_unix, static_cast<socklen_t>(sizeof(sock_unix))) == 0) {
        log_error("Socket file %s already in use by another process", socket_file.c_str());
        throw std::runtime_error("Socket file already in use");
      } else {
        if (errno == ECONNREFUSED) {
          log_warning("Socket file %s already exists, but seems to be unused. Deleting and retrying...", socket_file.c_str());
          if (unlink(socket_file.c_str()) == -1) {
            if (errno != ENOENT) {
              log_warning("%s", ("Failed removing socket file " + socket_file + " (" + get_strerror(errno) + " (" + to_string(errno) + "))").c_str());
              throw std::runtime_error(
                  "Failed removing socket file " + socket_file + " (" + get_strerror(errno) + " (" + to_string(errno) + "))");
            }
          }
          errno = 0;
          socket_operations_->close(service_named_socket_);
          if ((service_named_socket_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            throw std::runtime_error(get_strerror(errno));
          }
          goto retry;
        } else {
          errno = save_errno;
        }
      }
    }
    log_error("Error binding to socket file %s: %s", socket_file.c_str(), get_strerror(errno).c_str());
    throw std::runtime_error(get_strerror(errno));
  }

  if (listen(service_named_socket_, kListenQueueSize) < 0) {
    throw runtime_error("Failed to start listening for connections using named socket");
  }
}
#endif

void MySQLRouting::set_destinations_from_uri(const URI &uri) {
  if (uri.scheme == "metadata-cache") {
    // Syntax: metadata_cache://[<metadata_cache_key(unused)>]/<replicaset_name>?role=PRIMARY|SECONDARY|PRIMARY_AND_SECONDARY
    std::string replicaset_name = kDefaultReplicaSetName;

    if (uri.path.size() > 0 && !uri.path[0].empty())
      replicaset_name = uri.path[0];

    destination_.reset(new DestMetadataCacheGroup(uri.host, replicaset_name,
                                                  routing_strategy_,
                                                  uri.query, protocol_->get_type(),
                                                  access_mode_));
  } else {
    throw runtime_error(string_format("Invalid URI scheme; expecting: 'metadata-cache' is: '%s'",
                                      uri.scheme.c_str()));
  }
}

namespace {

routing::RoutingStrategy get_default_routing_strategy(const routing::AccessMode access_mode) {
  switch (access_mode) {
   case routing::AccessMode::kReadOnly:
    return routing::RoutingStrategy::kRoundRobin;
   case routing::AccessMode::kReadWrite:
    return routing::RoutingStrategy::kFirstAvailable;
   default:; // fall-through
  }

  // safe default if access_mode is also not specified
  return routing::RoutingStrategy::kFirstAvailable;
}

RouteDestination* create_standalone_destination(const routing::RoutingStrategy strategy,
                                                const Protocol::Type protocol,
                                                routing::SocketOperationsBase *sock_ops) {
  switch (strategy) {
    case RoutingStrategy::kFirstAvailable:
      return new DestFirstAvailable(protocol, sock_ops);
    case RoutingStrategy::kNextAvailable:
      return new DestNextAvailable(protocol, sock_ops);
    case RoutingStrategy::kRoundRobin:
      return new DestRoundRobin(protocol, sock_ops);
    case RoutingStrategy::kUndefined:
    case RoutingStrategy::kRoundRobinWithFallback:
      ; // unsupported, fall through
  }

  throw std::runtime_error("Wrong routing strategy " +
                           std::to_string(static_cast<int>(strategy)));
}
}

void MySQLRouting::set_destinations_from_csv(const string &csv) {
  std::stringstream ss(csv);
  std::string part;
  std::pair<std::string, uint16_t> info;

  // if no routing_strategy is defined for standalone routing
  // we set the default based on the mode
  if (routing_strategy_ == RoutingStrategy::kUndefined) {
    routing_strategy_ = get_default_routing_strategy(access_mode_);
  }

  destination_.reset(create_standalone_destination(routing_strategy_,
                                                   protocol_->get_type(),
                                                   socket_operations_));

  // Fall back to comma separated list of MySQL servers
  while (std::getline(ss, part, ',')) {
    info = mysqlrouter::split_addr_port(part);
    if (info.second == 0) {
      info.second = Protocol::get_default_port(protocol_->get_type());
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



std::chrono::milliseconds MySQLRouting::set_destination_connect_timeout(std::chrono::milliseconds timeout) {
  if (timeout <= std::chrono::milliseconds::zero()) {
    std::string error_msg("[" + name + "] tried to set destination_connect_timeout using invalid value, was " + std::to_string(timeout.count()) + " ms");
    throw std::invalid_argument(error_msg);
  }
  destination_connect_timeout_ = timeout;
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
