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

// TODO REFACTORING: "first-available" needs to be renamed to something that better describes its function.
//                   All related filenames and identifiers should be renamed.
//                   Also while at it, get_mysql_socket() should probably take TCPAddress by const reference

#include "dest_first_available.h"

#ifdef _WIN32
#  include "Winsock2.h"
#endif

#include "gtest/gtest.h"

class MockSocketOperations : public routing::SocketOperationsInterface {
 public:
  int get_mysql_socket(mysqlrouter::TCPAddress addr, int, bool = true) noexcept override {
    call_cnt_++;
    if (fails_todo_) {
      set_errno(ECONNREFUSED);
      fails_todo_--;
      return -1;  // -1 means server is unavailable
    } else {
      set_errno(0);

      // if addr string starts with a number, this will return it. Therefore it's
      // recommended that addr.addr is set to something like "42"
      return atoi(addr.addr.c_str());
    }
  }

  void set_errno(int err) {
    // set errno/Windows equivalent. At the time of writing, unit tests included here
    // will pass just fine without this, as they are too low-level and the errno is
    // checked at higher level. But to do an accurate mock, we should set this.
#ifdef _WIN32
    WSASetLastError(err);
#else
    errno = err;
#endif
  }

  void fail(int fail_cnt) {
    fails_todo_ = fail_cnt;
  }

  int call_cnt() {
    int cc = call_cnt_;
    call_cnt_ = 0;
    return cc;
  }

 private:
  int fails_todo_ = 0;
  int call_cnt_   = 0;
};

std::shared_ptr<MockSocketOperations> g_so(std::make_shared<MockSocketOperations>());

class FirstAvailableTest : public ::testing::Test {

 public:
  FirstAvailableTest() : dest_(g_so) {
    dest_.add("41", 1);
    dest_.add("42", 2);
    dest_.add("43", 3);
  }

  DestFirstAvailable& dest() {
    return dest_;
  }

 private:
  DestFirstAvailable dest_; // this is the class we're testing
};

// The idea behind these tests is to test DestFirstAvailable::get_server_socket() server selection
// strategy. That method is responsible for returning the new connection to the active server.
// The active server should be switched in such fashion:
//
//   A -> B -> C -> sorry, no more servers (regardless of whether A and B go back up or not)
//
// The switch should occur only when the current active server becomes unavailable.
// DestFirstAvailable::get_server_socket() relies on SocketOperationsInterface::get_mysql_socket()
// to return the actual file descriptor, which we mock in this test to simulate connection success
// or failure.

TEST_F(FirstAvailableTest, TypicalFailoverSequence) {
  int dummy;

  // talk to 1st server
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 41);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 41);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 41);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 41);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 41);
  ASSERT_EQ(g_so->call_cnt(), 5); // 5 good connections

  // fail 1st server -> failover to 2nd
  g_so->fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(g_so->call_cnt(), 2); // 1 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(g_so->call_cnt(), 4); // 4 more good conns

  // fail 2nd server -> failover to 3rd
  g_so->fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(g_so->call_cnt(), 2); // 1 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(g_so->call_cnt(), 4); // 4 more good conns

  // fail 3rd server -> no more servers
  g_so->fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(g_so->call_cnt(), 1); // 1 failed, no more servers
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(g_so->call_cnt(), 0); // no more servers
}

TEST_F(FirstAvailableTest, StartWith1stDown) {
  int dummy;

  // fail 1st server -> failover to 2nd
  g_so->fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(g_so->call_cnt(), 2); // 1 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(g_so->call_cnt(), 4); // 4 more good conns

  // fail 2nd server -> failover to 3rd
  g_so->fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(g_so->call_cnt(), 2); // 1 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(g_so->call_cnt(), 4); // 4 more good conns

  // fail 3rd server -> no more servers
  g_so->fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(g_so->call_cnt(), 1); // 1 failed, no more servers
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(g_so->call_cnt(), 0); // no more servers
}

TEST_F(FirstAvailableTest, StartWith2ndDown) {
  int dummy;

  // fail 1st and 2nd server -> failover to 3rd
  g_so->fail(2);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(g_so->call_cnt(), 3); // 2 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(g_so->call_cnt(), 4); // 4 more good conns

  // fail 3rd server -> no more servers
  g_so->fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(g_so->call_cnt(), 1); // 1 failed, no more servers
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(g_so->call_cnt(), 0); // no more servers
}

TEST_F(FirstAvailableTest, StartWithAllDown) {
  int dummy;

  // fail 1st, 2nd and 3rd server -> no more servers
  g_so->fail(3);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(g_so->call_cnt(), 3); // 3 failed, no more servers
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(g_so->call_cnt(), 0); // no more servers
}

