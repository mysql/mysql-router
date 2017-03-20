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

/**
 * BUG21962350 Issue with destination server removal from quarantine
 *
 */

#include "mysql/harness/config_parser.h"
#include "destination.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/utils.h"
#include "test/helpers.h"
#include "mysql/harness/loader.h"

#include <fstream>
#include <string>
#include <thread>
#include <vector>

//ignore GMock warnings
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif

#include "gmock/gmock.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

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
    orig_cerr_ = std::cerr.rdbuf(ssout.rdbuf());
  }

  virtual void TearDown() {
    if (orig_cerr_) {
      std::cerr.rdbuf(orig_cerr_);
    }
  }

  static const std::vector<TCPAddress> servers;

  std::stringstream ssout;

private:
  std::streambuf *orig_cerr_;
};

// NOTE: this test must run as first, it doesn't really test anything, just inits logger.
TEST_F(Bug21962350, InitLogger) {
  init_log();
}

TEST_F(Bug21962350, AddToQuarantine) {
  size_t exp;
  MockRouteDestination d;
  d.add(servers[0]);
  d.add(servers[1]);
  d.add(servers[2]);

  d.add_to_quarantine(static_cast<size_t>(0));
  ASSERT_THAT(ssout.str(), HasSubstr("Quarantine destination server s1.example.com:3306"));
  d.add_to_quarantine(static_cast<size_t>(1));
  exp = 2;
  ASSERT_EQ(exp, d.size_quarantine());
  ASSERT_THAT(ssout.str(), HasSubstr("s2.example.com:3306"));
  d.add_to_quarantine(static_cast<size_t>(2));
  ASSERT_THAT(ssout.str(), HasSubstr("s3.example.com:3306"));
  exp = 3;
  ASSERT_EQ(exp, d.size_quarantine());
}


TEST_F(Bug21962350, CleanupQuarantine) {
  size_t exp;
  ::testing::NiceMock<MockRouteDestination> d;
  d.add(servers[0]);
  d.add(servers[1]);
  d.add(servers[2]);

  d.add_to_quarantine(static_cast<size_t>(0));
  d.add_to_quarantine(static_cast<size_t>(1));
  d.add_to_quarantine(static_cast<size_t>(2));
  exp = 3;
  ASSERT_EQ(exp, d.size_quarantine());

  EXPECT_CALL(d, get_mysql_socket(_, _, _)).Times(4)
    .WillOnce(Return(100))
    .WillOnce(Return(-1))
    .WillOnce(Return(300))
    .WillOnce(Return(200));
  d.cleanup_quarantine();
  // Second is still failing
  exp = 1;
  ASSERT_EQ(exp, d.size_quarantine());
  // Next clean up should remove s2.example.com
  d.cleanup_quarantine();
  exp = 0;
  ASSERT_EQ(exp,d.size_quarantine());
  ASSERT_THAT(ssout.str(), HasSubstr("Unquarantine destination server s2.example.com:3306"));
}

TEST_F(Bug21962350, QuarantineServerMultipleTimes) {
  size_t exp;
  MockRouteDestination d;
  d.add(servers[0]);
  d.add(servers[1]);
  d.add(servers[2]);

  d.add_to_quarantine(static_cast<size_t>(0));
  d.add_to_quarantine(static_cast<size_t>(0));
  d.add_to_quarantine(static_cast<size_t>(2));
  d.add_to_quarantine(static_cast<size_t>(1));

  exp = 3;
  ASSERT_EQ(exp, d.size_quarantine());
}

#if !defined(_WIN32) && !defined(__FreeBSD__) && !defined(NDEBUG)
// This test doesn't work in Windows or FreeBSD, because of how ASSERT_DEATH works
// It also fails on release version
// But this test is gone in newer branches anyway, so disabling for now
TEST_F(Bug21962350, QuarantineServerNonExisting) {
  size_t exp;
  MockRouteDestination d;
  d.add(servers[0]);
  d.add(servers[1]);
  d.add(servers[2]);

  ASSERT_DEBUG_DEATH(d.add_to_quarantine(static_cast<size_t>(999)), ".*(index < size()).*");
  exp = 0;
  ASSERT_EQ(exp, d.size_quarantine());
}
#endif

TEST_F(Bug21962350, AlreadyQuarantinedServer) {
  size_t exp;
  MockRouteDestination d;
  d.add(servers[0]);
  d.add(servers[1]);
  d.add(servers[2]);

  d.add_to_quarantine(static_cast<size_t>(1));
  d.add_to_quarantine(static_cast<size_t>(1));
  exp = 1;
  ASSERT_EQ(exp, d.size_quarantine());
}

std::vector<TCPAddress> const Bug21962350::servers  {
  TCPAddress("s1.example.com", 3306),
  TCPAddress("s2.example.com", 3306),
  TCPAddress("s3.example.com", 3306),
};
