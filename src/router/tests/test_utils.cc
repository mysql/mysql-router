/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "mysqlrouter/utils.h"

const std::string kIPv6AddrRange = "fd84:8829:117d:63d5";

using mysqlrouter::split_addr_port;
using mysqlrouter::get_tcp_port;

class SplitAddrPortTest: public ::testing::Test {
protected:
  virtual void SetUp() {
  }
};

TEST_F(SplitAddrPortTest, SplitAddrPort) {
  std::string addr6 = kIPv6AddrRange + ":0001:0002:0003:0004";

  EXPECT_THAT(split_addr_port(addr6), ::testing::Pair(addr6, 0));
  EXPECT_THAT(split_addr_port("[" + addr6 + "]"), ::testing::Pair(addr6, 0));
  EXPECT_THAT(split_addr_port("[" + addr6 + "]:3306"), ::testing::Pair(addr6, 3306));

  EXPECT_THAT(split_addr_port("192.168.14.77"), ::testing::Pair("192.168.14.77", 0));
  EXPECT_THAT(split_addr_port("192.168.14.77:3306"), ::testing::Pair("192.168.14.77", 3306));

  EXPECT_THAT(split_addr_port("mysql.example.com"), ::testing::Pair("mysql.example.com", 0));
  EXPECT_THAT(split_addr_port("mysql.example.com:3306"), ::testing::Pair("mysql.example.com", 3306));
}

TEST_F(SplitAddrPortTest, SplitAddrPortFail) {
  std::string addr6 = kIPv6AddrRange + ":0001:0002:0003:0004";
  ASSERT_THROW(split_addr_port("[" + addr6), std::runtime_error);
  ASSERT_THROW(split_addr_port(addr6 + "]"), std::runtime_error);
  ASSERT_THROW(split_addr_port(kIPv6AddrRange + ":xyz00:0002:0003:0004"), std::runtime_error);

  // Invalid TCP port
  ASSERT_THROW(split_addr_port("192.168.14.77:999999"), std::runtime_error);
  ASSERT_THROW(split_addr_port("192.168.14.77:66000"), std::runtime_error);
  ASSERT_THROW(split_addr_port("[" + addr6 + "]:999999"), std::runtime_error);
}

class GetTCPPortTest: public ::testing::Test {
protected:
  virtual void SetUp() {
  }
};

TEST_F(GetTCPPortTest, GetTCPPort) {
  ASSERT_EQ(get_tcp_port("3306"), 3306);
  ASSERT_EQ(get_tcp_port("0"), 0);
  ASSERT_EQ(get_tcp_port(""), 0);
  ASSERT_EQ(get_tcp_port("65535"), 65535);
}

TEST_F(GetTCPPortTest, GetTCPPortFail) {
  ASSERT_THROW(get_tcp_port("65536"), std::runtime_error);
  ASSERT_THROW(get_tcp_port("33 06"), std::runtime_error);
  ASSERT_THROW(get_tcp_port(":3306"), std::runtime_error);
  ASSERT_THROW(get_tcp_port("99999999"), std::runtime_error);
  ASSERT_THROW(get_tcp_port("abcdef"), std::runtime_error);
}
