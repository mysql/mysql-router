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

// TODO REFACTORING: "first-available" needs to be renamed to something that better describes its function.
//                   All related filenames and identifiers should be renamed.
//                   Also while at it, get_mysql_socket() should probably take TCPAddress by const reference

#include "dest_first_available.h"
#include "routing_mocks.h"
#include "test/helpers.h"

class FirstAvailableTest : public ::testing::Test {

 public:
  FirstAvailableTest() : sock_ops_(new MockSocketOperations()),
                         dest_(Protocol::Type::kClassicProtocol, sock_ops_.get()) {
    dest_.add("41", 1);
    dest_.add("42", 2);
    dest_.add("43", 3);
  }

  DestFirstAvailable& dest() {
    return dest_;
  }

protected:
  std::unique_ptr<MockSocketOperations> sock_ops_;
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
// DestFirstAvailable::get_server_socket() relies on SocketOperationsBase::get_mysql_socket()
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
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 5); // 5 good connections

  // fail 1st server -> failover to 2nd
  sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 2); // 1 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 4); // 4 more good conns

  // fail 2nd server -> failover to 3rd
  sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 2); // 1 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 4); // 4 more good conns

  // fail 3rd server -> no more servers
  sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 1); // 1 failed, no more servers
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 0); // no more servers
}

TEST_F(FirstAvailableTest, StartWith1stDown) {
  int dummy;

  // fail 1st server -> failover to 2nd
  sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 2); // 1 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 42);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 4); // 4 more good conns

  // fail 2nd server -> failover to 3rd
  sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 2); // 1 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 4); // 4 more good conns

  // fail 3rd server -> no more servers
  sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 1); // 1 failed, no more servers
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 0); // no more servers
}

TEST_F(FirstAvailableTest, StartWith2ndDown) {
  int dummy;

  // fail 1st and 2nd server -> failover to 3rd
  sock_ops_->get_mysql_socket_fail(2);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 3); // 2 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), 43);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 4); // 4 more good conns

  // fail 3rd server -> no more servers
  sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 1); // 1 failed, no more servers
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 0); // no more servers
}

TEST_F(FirstAvailableTest, StartWithAllDown) {
  int dummy;

  // fail 1st, 2nd and 3rd server -> no more servers
  sock_ops_->get_mysql_socket_fail(3);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 3); // 3 failed, no more servers
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(0, &dummy), -1);
  ASSERT_EQ(sock_ops_->get_mysql_socket_call_cnt(), 0); // no more servers
}

int main(int argc, char *argv[]) {
  init_log();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

