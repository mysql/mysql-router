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

/**
 * BUG21962350 Issue with destination server removal from quarantine
 *
 */

#include "mysqlrouter/routing.h"
#include "mysqlrouter/utils.h"

#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "gmock/gmock.h"
#include "config_parser.h"
#include "helper_logger.h"

#include "destination.h"

using mysqlrouter::TCPAddress;
using mysqlrouter::to_string;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::Eq;
using ::testing::_;

class MockRouteDestination : public RouteDestination {
public:
  void add_to_quarantine(const size_t index) noexcept {
    RouteDestination::add_to_quarantine(index);
  }

  void cleanup_quarantine() noexcept {
    RouteDestination::cleanup_quarantine();
  }

  MOCK_METHOD3(get_mysql_socket, int(const TCPAddress &addr, int connect_timeout, bool log_errors));
};

class Bug21962350 : public ::testing::Test {
protected:
  virtual void SetUp() {
    orig_cout_ = std::cout.rdbuf(ssout.rdbuf());
  }

  virtual void TearDown() {
    if (orig_cout_) {
      std::cout.rdbuf(orig_cout_);
    }
  }

  static const std::vector<TCPAddress> servers;

  std::stringstream ssout;

private:
  std::streambuf *orig_cout_;
};

TEST_F(Bug21962350, AddToQuarantine) {
  MockRouteDestination d;
  d.add(servers[0]);
  d.add(servers[1]);
  d.add(servers[2]);

  d.add_to_quarantine(0);
  ASSERT_THAT(ssout.str(), HasSubstr("Quarantine destination server s1.example.com:3306"));
  d.add_to_quarantine(1);
  ASSERT_EQ(2, d.size_quarantine());
  ASSERT_THAT(ssout.str(), HasSubstr("s2.example.com:3306"));
  d.add_to_quarantine(2);
  ASSERT_THAT(ssout.str(), HasSubstr("s3.example.com:3306"));
  ASSERT_EQ(3, d.size_quarantine());
}


TEST_F(Bug21962350, CleanupQuarantine) {
  MockRouteDestination d;
  d.add(servers[0]);
  d.add(servers[1]);
  d.add(servers[2]);

  d.add_to_quarantine(0);
  d.add_to_quarantine(1);
  d.add_to_quarantine(2);
  ASSERT_EQ(3, d.size_quarantine());

  EXPECT_CALL(d, get_mysql_socket(_, _, _)).Times(4)
    .WillOnce(Return(100))
    .WillOnce(Return(-1))
    .WillOnce(Return(300))
    .WillOnce(Return(200));
  d.cleanup_quarantine();
  // Second is still failing
  size_t exp = 1;
  ASSERT_EQ(exp, d.size_quarantine());
  // Next clean up should remove s2.example.com
  d.cleanup_quarantine();
  ASSERT_EQ(0, d.size_quarantine());
  ASSERT_THAT(ssout.str(), HasSubstr("Unquarantine destination server s2.example.com:3306"));
}

TEST_F(Bug21962350, QuarantineServerMultipleTimes) {
  MockRouteDestination d;
  d.add(servers[0]);
  d.add(servers[1]);
  d.add(servers[2]);

  d.add_to_quarantine(0);
  d.add_to_quarantine(0);
  d.add_to_quarantine(2);
  d.add_to_quarantine(1);

  ASSERT_EQ(3, d.size_quarantine());
}

TEST_F(Bug21962350, QuarantineServerNonExisting) {
  MockRouteDestination d;
  d.add(servers[0]);
  d.add(servers[1]);
  d.add(servers[2]);

  ASSERT_DEATH(d.add_to_quarantine(999), ".*(index < size()).*");
  ASSERT_EQ(0, d.size_quarantine());
}

TEST_F(Bug21962350, AlreadyQuarantinedServer) {
  MockRouteDestination d;
  d.add(servers[0]);
  d.add(servers[1]);
  d.add(servers[2]);

  d.add_to_quarantine(1);
  d.add_to_quarantine(1);
  ASSERT_EQ(1, d.size_quarantine());
}

std::vector<TCPAddress> const Bug21962350::servers  {
  TCPAddress("s1.example.com", 3306),
  TCPAddress("s2.example.com", 3306),
  TCPAddress("s3.example.com", 3306),
};