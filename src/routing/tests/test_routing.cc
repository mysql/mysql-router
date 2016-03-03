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

#include "gmock/gmock.h"

#include <sys/fcntl.h>
#include <sys/socket.h>

#include "mysqlrouter/routing.h"

using routing::AccessMode;
using routing::set_socket_blocking;
using ::testing::ContainerEq;
using ::testing::StrEq;

class RoutingTests : public ::testing::Test {
protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
};

TEST_F(RoutingTests, AccessModes) {
  ASSERT_EQ(static_cast<int>(AccessMode::kReadWrite), 1);
  ASSERT_EQ(static_cast<int>(AccessMode::kReadOnly), 2);
}

TEST_F(RoutingTests, AccessModeLiteralNames) {
  std::map<string, AccessMode> exp = {
      {"read-write", AccessMode::kReadWrite},
      {"read-only",  AccessMode::kReadOnly},
  };
  ASSERT_THAT(routing::kAccessModeNames, ContainerEq(exp));
}

TEST_F(RoutingTests, GetAccessLiteralName) {
  using routing::get_access_mode_name;
  ASSERT_THAT(get_access_mode_name(AccessMode::kReadWrite), StrEq("read-write"));
  ASSERT_THAT(get_access_mode_name(AccessMode::kReadOnly), StrEq("read-only"));
}

TEST_F(RoutingTests, Defaults) {
  ASSERT_EQ(routing::kDefaultWaitTimeout, 0);
  ASSERT_EQ(routing::kDefaultMaxConnections, 512);
  ASSERT_EQ(routing::kDefaultDestinationConnectionTimeout, 1);
  ASSERT_EQ(routing::kDefaultBindAddress, "127.0.0.1");
  ASSERT_EQ(routing::kDefaultNetBufferLength, 16384U);
  ASSERT_EQ(routing::kDefaultMaxConnectErrors, 100ULL);
  ASSERT_EQ(routing::kDefaultClientConnectTimeout, 9UL);
}


TEST_F(RoutingTests, SetSocketBlocking) {
  int s = socket(PF_INET, SOCK_STREAM, 6);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_NONBLOCK, 0);
  set_socket_blocking(s, false);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_NONBLOCK, O_NONBLOCK);
  set_socket_blocking(s, true);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_NONBLOCK, 0) << std::endl;

  fcntl(s, F_SETFL, O_RDONLY);
  set_socket_blocking(s, false);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_NONBLOCK, O_NONBLOCK);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_RDONLY, O_RDONLY);
}

