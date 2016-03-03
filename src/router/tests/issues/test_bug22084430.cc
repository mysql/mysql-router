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
 * BUG22084430 IPV6 ADDRESS IN LOGS DOES NOT USE []
 *
 */

#include "gmock/gmock.h"

#include "mysqlrouter/datatypes.h"
using mysqlrouter::TCPAddress;

class Bug22084430 : public ::testing::Test {
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(Bug22084430, LogCorrectIPv6Address) {
  std::map<std::string, TCPAddress> address{
    {"[::]:7002", TCPAddress ("::", 7002)},
    {"[FE80:0000:0000:0000:0202:B3FF:FE1E:8329]:8329", TCPAddress ("FE80:0000:0000:0000:0202:B3FF:FE1E:8329", 8329)},
    {"[FE80::0202:B3FF:FE1E:8329]:80", TCPAddress ("FE80::0202:B3FF:FE1E:8329", 80)},
  };

  for (auto &it: address) {
    EXPECT_EQ(it.second.str(), it.first);
  }
}

TEST_F(Bug22084430, LogCorrectIPv4Address) {
  std::map<std::string, TCPAddress> address{
    {"127.0.0.1:7002", TCPAddress ("127.0.0.1", 7002)},
    {"192.168.1.128:8329", TCPAddress ("192.168.1.128", 8329)},
  };

  for (auto &it: address) {
    EXPECT_EQ(it.second.str(), it.first);
  }
}
