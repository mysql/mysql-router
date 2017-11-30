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

#ifndef ROUTING_MOCKS_INCLUDED
#define ROUTING_MOCKS_INCLUDED

#ifdef _WIN32
#  include "Winsock2.h"
#endif

//ignore GMock warnings
#ifdef __clang__
#  ifndef __has_warning
#    define __has_warning(x) 0
#  endif
#  pragma clang diagnostic push
#  if __has_warning("-Winconsistent-missing-override")
#    pragma clang diagnostic ignored "-Winconsistent-missing-override"
#  endif
#  if __has_warning("-Wsign-conversion")
#    pragma clang diagnostic ignored "-Wsign-conversion"
#  endif
#endif
#include "gmock/gmock.h"
#include "gtest/gtest.h"


class MockSocketOperations : public routing::SocketOperationsBase {
 public:
  int get_mysql_socket(mysqlrouter::TCPAddress addr, std::chrono::milliseconds, bool = true) noexcept override {
    get_mysql_socket_call_cnt_++;
    if (get_mysql_socket_fails_todo_) {
      set_errno(ECONNREFUSED);
      get_mysql_socket_fails_todo_--;
      return -1;  // -1 means server is unavailable
    } else {
      set_errno(0);

      // if addr string starts with a number, this will return it. Therefore it's
      // recommended that addr.addr is set to something like "42"
      return atoi(addr.addr.c_str());
    }
  }

  MOCK_METHOD3(read, ssize_t(int, void*, size_t));
  MOCK_METHOD3(write, ssize_t(int, void*, size_t));
  MOCK_METHOD1(close, void(int));
  MOCK_METHOD1(shutdown, void(int));
  MOCK_METHOD1(freeaddrinfo, void(addrinfo *ai));
  MOCK_METHOD4(getaddrinfo, int(const char*, const char*, const addrinfo*, addrinfo**));
  MOCK_METHOD3(bind, int(int, const struct sockaddr*, socklen_t));
  MOCK_METHOD3(socket, int(int, int, int));
  MOCK_METHOD5(setsockopt, int(int, int, int, const void*, socklen_t));
  MOCK_METHOD2(listen, int(int fd, int n));
  MOCK_METHOD3(poll, int(struct pollfd *, nfds_t, std::chrono::milliseconds));

  void set_errno(int err) override {
    // set errno/Windows equivalent. At the time of writing, unit tests
    // will pass just fine without this, as they are too low-level and the errno is
    // checked at higher level. But to do an accurate mock, we should set this.
#ifdef _WIN32
    WSASetLastError(err);
#else
    errno = err;
#endif
  }

  int get_errno() override {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
  }

  void get_mysql_socket_fail(int fail_cnt) {
    get_mysql_socket_fails_todo_ = fail_cnt;
  }

  int get_mysql_socket_call_cnt() {
    int cc = get_mysql_socket_call_cnt_;
    get_mysql_socket_call_cnt_ = 0;
    return cc;
  }

 private:
  int get_mysql_socket_fails_todo_ = 0;
  int get_mysql_socket_call_cnt_   = 0;
};

#ifdef __clang__
#  pragma clang diagnostic pop
#endif


#endif // ROUTING_MOCKS_INCLUDED
