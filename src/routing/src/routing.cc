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

#include "mysqlrouter/routing.h"

#include <cstring>

#ifdef __sun
#include <fcntl.h>
#else

#include <sys/fcntl.h>

#endif

#include <netdb.h>
#include <netinet/tcp.h>

#include <sys/socket.h>

#include "mysqlrouter/utils.h"
#include "logger.h"
#include "utils.h"

using mysqlrouter::to_string;
using mysqlrouter::string_format;
using mysqlrouter::TCPAddress;

namespace routing {

const int kDefaultWaitTimeout = 0; // 0 = no timeout used
const int kDefaultMaxConnections = 512;
const int kDefaultDestinationConnectionTimeout = 1;
const string kDefaultBindAddress = "127.0.0.1";
const unsigned int kDefaultNetBufferLength = 16384;  // Default defined in latest MySQL Server
const unsigned long long kDefaultMaxConnectErrors = 100;  // Similar to MySQL Server
const unsigned int kDefaultClientConnectTimeout = 9; // Default connect_timeout MySQL Server minus 1

const std::map<string, AccessMode> kAccessModeNames = {
    {"read-write", AccessMode::kReadWrite},
    {"read-only",  AccessMode::kReadOnly},
};

string get_access_mode_name(AccessMode access_mode) noexcept {
  for (auto &it: kAccessModeNames) {
    if (it.second == access_mode) {
      return it.first;
    }
  }
  return "";
}

void set_socket_blocking(int sock, bool blocking) {

  assert(!(sock < 0));

  auto flags = fcntl(sock, F_GETFL, nullptr);
  assert(flags >= 0);
  if (blocking) {
    flags &= ~O_NONBLOCK;
  } else {
    flags |= O_NONBLOCK;
  }
  fcntl(sock, F_SETFL, flags);
}

int get_mysql_socket(TCPAddress addr, int connect_timeout, bool log) noexcept {
  fd_set readfds;
  fd_set writefds;
  struct timeval timeout_val;

  struct addrinfo *servinfo, *info, hints;

  int opt_nodelay = 1;
  int res;
  int so_error = 0;
  int sock = -1;
  socklen_t error_len = sizeof(so_error);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int err;
  if ((err = getaddrinfo(addr.addr.c_str(), to_string(addr.port).c_str(), &hints, &servinfo)) != 0) {
    if (log) {
      std::string errstr{(err == EAI_SYSTEM) ? strerror(errno) : gai_strerror(err)};
      log_debug("Failed getting address information for '%s' (%s)", addr.addr.c_str(), errstr.c_str());
    }
    return -1;
  }

  errno = 0;
  for (info = servinfo; info != nullptr; info = info->ai_next) {
    if ((sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
      log_error("Failed opening socket: %s", strerror(errno));
      continue;
    }
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    writefds = readfds;
    timeout_val.tv_sec = connect_timeout;
    timeout_val.tv_usec = 0;

    // Set non-blocking so we can timeout using select()
    set_socket_blocking(sock, false);
    if (connect(sock, info->ai_addr, info->ai_addrlen) < 0) {
      if (errno != EINPROGRESS) {
        close(sock);
        continue;
      }
    }

    res = select(sock + 1, &readfds, &writefds, nullptr, &timeout_val);
    if (res <= 0) {
      if (res == 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        if (log) {
          log_debug("Timeout reached trying to connect to MySQL Server %s", addr.str().c_str());
        }
        continue;
      }
      log_debug("select failed");
      continue;
    }

    if (FD_ISSET(sock, &readfds) || FD_ISSET(sock, &writefds)) {
      if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &error_len) == -1) {
        log_debug("Failed executing getsockopt on client socket: %s", strerror(errno));
        continue;
      }
    } else {
      log_debug("Failed connecting with MySQL server %s", addr.str().c_str());
      continue;
    }
    break;
  }

  if (info == nullptr) {
    return -1;
  } else if (servinfo) {
    freeaddrinfo(servinfo);
  }

  // Handle remaining errors
  if ((errno > 0 && errno != EINPROGRESS) || so_error) {
    shutdown(sock, SHUT_RDWR);
    close(sock);
    err = so_error ? so_error : errno;
    if (log) {
      log_debug("MySQL Server %s: %s (%d)", addr.str().c_str(), strerror(err), err);
    }
    return -1;
  }

  set_socket_blocking(sock, false);
  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt_nodelay, sizeof(int)) == -1) {
    log_debug("Failed setting TCP_NODELAY on client socket");
    return -1;
  }

  errno = 0;
  return sock;
}

} // routing
