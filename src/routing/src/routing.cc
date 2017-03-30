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
#include "mysqlrouter/utils.h"
#include "config.h"
#include "mysql/harness/logging.h"
#include "utils.h"

#include <cstring>

#ifndef _WIN32
# ifdef __sun
#  include <fcntl.h>
# else
#  include <sys/fcntl.h>
# endif
# include <netdb.h>
# include <netinet/tcp.h>
# include <sys/socket.h>
#else
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <winsock2.h>
# include <ws2tcpip.h>
#endif

using mysqlrouter::to_string;
using mysqlrouter::string_format;
using mysqlrouter::TCPAddress;
IMPORT_LOG_FUNCTIONS()

namespace routing {

const int kDefaultWaitTimeout = 0; // 0 = no timeout used
const int kDefaultMaxConnections = 512;
const int kDefaultDestinationConnectionTimeout = 1;
const std::string kDefaultBindAddress = "127.0.0.1";
const unsigned int kDefaultNetBufferLength = 16384;  // Default defined in latest MySQL Server
const unsigned long long kDefaultMaxConnectErrors = 100;  // Similar to MySQL Server
const unsigned int kDefaultClientConnectTimeout = 9; // Default connect_timeout MySQL Server minus 1

const char* const kAccessModeNames[] = {
  nullptr, "read-write", "read-only"
};

constexpr size_t kAccessModeCount =
    sizeof(kAccessModeNames)/sizeof(*kAccessModeNames);

AccessMode get_access_mode(const std::string& value) {
  for (unsigned int i = 1 ; i < kAccessModeCount ; ++i)
    if (strcmp(kAccessModeNames[i], value.c_str()) == 0)
      return static_cast<AccessMode>(i);
  return AccessMode::kUndefined;
}

void get_access_mode_names(std::string* valid) {
  unsigned int i = 1;
  while (i < kAccessModeCount) {
    valid->append(kAccessModeNames[i]);
    if (++i < kAccessModeCount)
      valid->append(", ");
  }
}

std::string get_access_mode_name(AccessMode access_mode) noexcept {
  return kAccessModeNames[static_cast<int>(access_mode)];
}

void set_socket_blocking(int sock, bool blocking) {

  assert(!(sock < 0));
#ifndef _WIN32
  auto flags = fcntl(sock, F_GETFL, nullptr);
  assert(flags >= 0);
  if (blocking) {
    flags &= ~O_NONBLOCK;
  } else {
    flags |= O_NONBLOCK;
  }
  fcntl(sock, F_SETFL, flags);
#else
  u_long mode = blocking ? 0 : 1;
  ioctlsocket(sock, FIONBIO, &mode);
#endif
}

SocketOperations* SocketOperations::instance() {
  static SocketOperations instance_;
  return &instance_;
}

int SocketOperations::get_mysql_socket(TCPAddress addr, int connect_timeout, bool log) noexcept {
  fd_set readfds;
  fd_set writefds;
  fd_set errfds;
  struct timeval timeout_val;

  struct addrinfo *servinfo, *info, hints;

  int opt_nodelay = 1;
  int res;
  int so_error = 0;
  int sock = -1;
  socklen_t error_len = static_cast<socklen_t>(sizeof(so_error));

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int err;
  if ((err = getaddrinfo(addr.addr.c_str(), to_string(addr.port).c_str(), &hints, &servinfo)) != 0) {
    if (log) {
#ifndef _WIN32
      std::string errstr{(err == EAI_SYSTEM) ? get_message_error(errno) : gai_strerror(err)};
#else
      std::string errstr = get_message_error(err);
#endif
      log_debug("Failed getting address information for '%s' (%s)", addr.addr.c_str(), errstr.c_str());
    }
    return -1;
  }

#ifdef _WIN32
  WSASetLastError(0);
#else
  errno = 0;
#endif
  for (info = servinfo; info != nullptr; info = info->ai_next) {
    if ((sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
      log_error("Failed opening socket: %s", get_message_error(errno).c_str());
      continue;
    }
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    errfds = writefds = readfds;
    timeout_val.tv_sec = connect_timeout;
    timeout_val.tv_usec = 0;

    // Set non-blocking so we can timeout using select()
    set_socket_blocking(sock, false);
    if (connect(sock, info->ai_addr, info->ai_addrlen) < 0) {
#ifdef _WIN32
      if (WSAGetLastError() != WSAEINPROGRESS && WSAGetLastError() != WSAEWOULDBLOCK) {
        log_error("Error connecting socket to %s:%i (%s)", addr.addr.c_str(), addr.port, get_message_error(SOCKET_ERROR).c_str());
        this->close(sock);
        continue;
      }
#else
      if (errno != EINPROGRESS) {
        log_error("Error connecting socket to %s:%i (%s)", addr.addr.c_str(), addr.port, strerror(errno));
        this->close(sock);
        continue;
      }
#endif
    }

    res = select(sock + 1, &readfds, &writefds, &errfds, &timeout_val);
    if (res <= 0) {
      this->shutdown(sock);
      this->close(sock);
      if (res == 0) {
        if (log) {
          log_warning("Timeout reached trying to connect to MySQL Server %s", addr.str().c_str());
        }
        continue;
      }
      log_debug("select failed");
      continue;
    }

    if (FD_ISSET(sock, &readfds) || FD_ISSET(sock, &writefds) || FD_ISSET(sock, &errfds)) {
      if (getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&so_error), &error_len) == -1) {
        log_debug("Failed executing getsockopt on client socket: %s",
          get_message_error(errno).c_str());
        this->shutdown(sock);
        this->close(sock);
        continue;
      }
      if (so_error) {
        log_debug("Socket error: %s: %s (%d)", addr.str().c_str(), get_message_error(so_error).c_str(), so_error);
        this->shutdown(sock);
        this->close(sock);
        continue;
      }
    } else {
      log_debug("Failed connecting with MySQL server %s", addr.str().c_str());
      this->shutdown(sock);
      this->close(sock);
      continue;
    }
    break;
  } // for (info = servinfo; info != nullptr; info = info->ai_next)

  if (info == nullptr) {
    return -1;
  } else if (servinfo) {
    freeaddrinfo(servinfo);
  }

  // Handle remaining errors
#ifdef _WIN32
  if ((WSAGetLastError() > 0 && WSAGetLastError() != WSAEINPROGRESS) || so_error) {
    this->shutdown(sock);
    this->close(sock);
    err = so_error ? so_error : SOCKET_ERROR;
    if (log) {
      log_debug("MySQL Server %s: %s (%d)", addr.str().c_str(), get_message_error(err).c_str(), err);
    }
    return -1;
  }
#else
  if ((errno > 0 && errno != EINPROGRESS) || so_error) {
    this->shutdown(sock);
    this->close(sock);
    err = so_error ? so_error : errno;
    if (log) {
      log_debug("MySQL Server %s: %s (%d)", addr.str().c_str(), get_message_error(err).c_str(), err);
    }
    return -1;
  }
#endif

  // set blocking; MySQL protocol is blocking and we do not take advantage of
  // any non-blocking possibilities
  set_socket_blocking(sock, true);

  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<const char*>(&opt_nodelay), // cast keeps Windows happy (const void* on Unix)
                 static_cast<socklen_t>(sizeof(int))) == -1) {
    log_debug("Failed setting TCP_NODELAY on client socket");
    return -1;
  }

#ifdef _WIN32
  WSASetLastError(0);
#else
  errno = 0;
#endif
  return sock;
}

ssize_t SocketOperations::write(int fd, void *buffer, size_t nbyte) {
#ifndef _WIN32
  return ::write(fd, buffer, nbyte);
#else
  return ::send(fd, reinterpret_cast<const char *>(buffer), nbyte, 0);
#endif
}

ssize_t SocketOperations::read(int fd, void *buffer, size_t nbyte) {
#ifndef _WIN32
  return ::read(fd, buffer, nbyte);
#else
  return ::recv(fd, reinterpret_cast<char *>(buffer), nbyte, 0);
#endif
}

void SocketOperations::close(int fd) {
#ifndef _WIN32
  ::close(fd);
#else
  ::closesocket(fd);
#endif
}

void SocketOperations::shutdown(int fd) {
#ifndef _WIN32
  ::shutdown(fd, SHUT_RDWR);
#else
  ::shutdown(fd, SD_BOTH);
#endif
}

} // routing
