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

#ifndef MYSQLROUTER_ROUTING_INCLUDED
#define MYSQLROUTER_ROUTING_INCLUDED

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/plugin_config.h"

#include <map>
#include <string>

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <poll.h>
#endif

#ifdef _WIN32
typedef ULONG nfds_t;
typedef long ssize_t;
#endif

namespace routing {

/** @brief Timeout for idling clients (in seconds)
 *
 * Constant defining how long (in seconds) a client can keep the connection idling. This is similar to the
 * wait_timeout variable in the MySQL Server.
 */
extern const int kDefaultWaitTimeout;

/** @brief Max number of active routes for this routing instance */
extern const int kDefaultMaxConnections;

/** @brief Timeout connecting to destination (in seconds)
 *
 * Constant defining how long we wait to establish connection with the server before we give up.
 */
extern const int kDefaultDestinationConnectionTimeout;

/** @brief Maximum connect or handshake errors per host
 *
 * Maximum connect or handshake errors after which a host will be
 * blocked. Such errors can happen when the client does not reply
 * the handshake, sends an incorrect packet, or garbage.
 *
 */
extern const unsigned long long kDefaultMaxConnectErrors;

/** @brief Maximum connect or handshake errors per host
 *
 * Maximum connect or handshake errors after which a host will be
 * blocked. Such errors can happen when the client does not reply
 * the handshake, sends an incorrect packet, or garbage.
 *
 */
extern const unsigned long long kDefaultMaxConnectErrors;

/** @brief Default bind address
 *
 */
extern const std::string kDefaultBindAddress;

/** @brief Default net buffer length
 *
 * Default network buffer length which can be set in the MySQL Server.
 *
 * This should match the default of the latest MySQL Server.
 */
extern const unsigned int kDefaultNetBufferLength;

/** @brief Timeout waiting for handshake response from client
 *
 * The number of seconds that MySQL Router waits for a handshake response.
 * The default value is 9 seconds (default MySQL Server minus 1).
 *
 */
extern const unsigned int kDefaultClientConnectTimeout;

#ifdef _WIN32
  const SOCKET kInvalidSocket = INVALID_SOCKET;// windows defines INVALID_SOCKET already
#else
  const int kInvalidSocket = -1;
#endif


/** @brief Modes supported by Routing plugin */
enum class AccessMode {
  kUndefined = 0,
  kReadWrite = 1,
  kReadOnly = 2,
};

void get_access_mode_names(std::string*);
AccessMode get_access_mode(const std::string&);

/** @brief Returns literal name of given access mode
 *
 * Returns literal name of given access mode as a std:string. When
 * the access mode is not found, empty string is returned.
 *
 * @param access_mode Access mode to look up
 * @return Name of access mode as std::string or empty string
 */
std::string get_access_mode_name(AccessMode access_mode) noexcept;

/**
 * Sets blocking flag for given socket
 *
 * @param sock a socket file descriptor
 * @param blocking whether to set blocking off (false) or on (true)
 */
void set_socket_blocking(int sock, bool blocking);

/** @class SocketOperationsBase
 * @brief Base class to allow multiple SocketOperations implementations
 *        (at least one "real" and one mock for testing purposes)
 */
class SocketOperationsBase {
 public:

  virtual ~SocketOperationsBase() = default;
  virtual int get_mysql_socket(mysqlrouter::TCPAddress addr, int connect_timeout, bool log = true) noexcept = 0;
  virtual ssize_t write(int  fd, void *buffer, size_t nbyte) = 0;
  virtual ssize_t read(int fd, void *buffer, size_t nbyte) = 0;
  virtual void close(int fd) = 0;
  virtual void shutdown(int fd) = 0;
  virtual void freeaddrinfo(addrinfo *ai) = 0;
  virtual int getaddrinfo(const char *node, const char *service, const addrinfo *hints, addrinfo **res) = 0;
  virtual int bind(int fd, const struct sockaddr *addr, socklen_t len) = 0;
  virtual int socket(int domain, int type, int protocol) = 0;
  virtual int setsockopt(int fd, int level, int optname,
                         const void *optval, socklen_t optlen) = 0;
  virtual int listen(int fd, int n) = 0;

  /** @brief Wrapper around socket library write() with a looping logic
   *         making sure the whole buffer got written
   */
  virtual ssize_t write_all(int fd, void *buffer, size_t nbyte) {
    ssize_t written = 0;
    size_t buffer_offset = 0;
    while (buffer_offset < nbyte) {
      if ((written = this->write(fd, reinterpret_cast<char*>(buffer)+buffer_offset, nbyte-buffer_offset)) < 0) {
        return -1;
      }
      buffer_offset += static_cast<size_t>(written);
    }
    return static_cast<ssize_t>(nbyte);
  }
  virtual int get_errno() = 0;
  virtual void set_errno(int) = 0;
  virtual int poll(struct pollfd *fds, nfds_t nfds, int timeout_ms) = 0;
};

/** @class SocketOperations
 * @brief This class provides a "real" (not mock) implementation
 */
class SocketOperations : public SocketOperationsBase {
 public:
  static SocketOperations* instance();

  /** @brief Returns socket descriptor of connected MySQL server
   *
   * Returns a socket descriptor for the connection to the MySQL Server or
   * -1 when an error occurred.
   *
   * @param addr information of the server we connect with
   * @param connect_timeout number of seconds waiting for connection
   * @param log whether to log errors or not
   * @return a socket descriptor
   */
  int get_mysql_socket(mysqlrouter::TCPAddress addr, int connect_timeout, bool log = true) noexcept override;

  /** @brief Thin wrapper around socket library write() */
  ssize_t write(int fd, void *buffer, size_t nbyte) override;

  /** @brief Thin wrapper around socket library read() */
  ssize_t read(int fd, void *buffer, size_t nbyte) override;

  /** @brief Thin wrapper around socket library close() */
  void close(int fd)  override;

  /** @brief Thin wrapper around socket library shutdown() */
  void shutdown(int fd) override;

  /** @brief Thin wrapper around socket library freeaddrinfo() */
  void freeaddrinfo(addrinfo *ai) override;

  /** @brief Thin wrapper around socket library getaddrinfo() */
  int getaddrinfo(const char *node, const char *service, const addrinfo *hints, addrinfo **res) override;

  /** @brief Thin wrapper around socket library bind() */
  int bind(int fd, const struct sockaddr *addr, socklen_t len) override;

  /** @brief Thin wrapper around socket library socket() */
  int socket(int domain, int type, int protocol) override;

  /** @brief Thin wrapper around socket library setsockopt() */
  int setsockopt(int fd, int level, int optname,
                 const void *optval, socklen_t optlen) override;

  /** @brief Thin wrapper around socket library listen() */
  int listen(int fd, int n) override;

  /**
   * wrapper around poll()/WSAPoll()
   */
  int poll(struct pollfd *fds, nfds_t nfds, int timeout_ms) override;

  /**
   * wait for a non-blocking connect() to finish
   *
   * @param sock a connected socket
   * @param timeout_ms time to wait for the connect to complete in milliseconds
   *
   * call connect_non_blocking_status() to get the final result
   */
  int connect_non_blocking_wait(int sock, int timeout_ms);

  /**
   * get the non-blocking connect() status
   *
   * must be called after connect()ed socket became writable.
   *
   * @see connect_non_blocking_wait() and poll()
   */
  int connect_non_blocking_status(int sock, int &so_error);

  /**
   * get the error-code of the last (socket) operation
   *
   * @see errno or WSAGetLastError()
   */
  int get_errno() override {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
  }

  /**
   * wrapper around errno/WSAGetLastError()
   */
  void set_errno(int e) override {
#ifdef _WIN32
    WSASetLastError(e);
#else
    errno = e;
#endif
  }
 private:
  SocketOperations(const SocketOperations&) = delete;
  SocketOperations operator=(const SocketOperations&) = delete;
  SocketOperations() = default;
};

} // namespace routing

#endif // MYSQLROUTER_ROUTING_INCLUDED
