/*
  Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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
#  include "Winsock2.h"
#endif

#include "gtest/gtest.h"
#include "gmock/gmock.h"

class MockSocketOperations : public routing::SocketOperationsBase {
 public:
  int get_mysql_socket(mysqlrouter::TCPAddress addr, int, bool = true) noexcept override {
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

  void set_errno(int err) {
    // set errno/Windows equivalent. At the time of writing, unit tests
    // will pass just fine without this, as they are too low-level and the errno is
    // checked at higher level. But to do an accurate mock, we should set this.
#ifdef _WIN32
    WSASetLastError(err);
#else
    errno = err;
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
