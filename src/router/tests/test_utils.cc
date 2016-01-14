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

#include "mysqlrouter/utils.h"

#include <vector>

const std::string kIPv6AddrRange = "fd84:8829:117d:63d5";

using mysqlrouter::split_addr_port;
using mysqlrouter::get_tcp_port;
using mysqlrouter::hexdump;
using mysqlrouter::split_string;
using ::testing::ContainerEq;
using ::testing::Pair;
using std::string;

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

class HexDumpTest: public ::testing::Test { };

TEST_F(HexDumpTest, UsingCharArray) {
  const unsigned char buffer[4] = "abc";
  EXPECT_EQ("61 62 63 \n", hexdump(buffer, 3, 0));
}

TEST_F(HexDumpTest, UsingVector) {
  std::vector<uint8_t> buffer = {'a', 'b', 'c'};
  EXPECT_EQ("61 62 63 \n", hexdump(&buffer[0], 3, 0));
}

TEST_F(HexDumpTest, Literals) {
  const unsigned char buffer[4] = "abc";
  EXPECT_EQ(" a  b  c \n", hexdump(buffer, 3, 0, true));
  EXPECT_EQ("61 62 63 \n", hexdump(buffer, 3, 0, false));
}

TEST_F(HexDumpTest, Count) {
  const unsigned char buffer[7] = "abcdef";
  EXPECT_EQ(" a  b  c  d  e  f \n", hexdump(buffer, 6, 0, true));
  EXPECT_EQ(" a  b  c \n", hexdump(buffer, 3, 0, true));
}

TEST_F(HexDumpTest, Start) {
  const unsigned char buffer[7] = "abcdef";
  EXPECT_EQ(" a  b  c  d  e  f \n", hexdump(buffer, 6, 0, true));
  EXPECT_EQ(" d  e  f \n", hexdump(buffer, 3, 3, true));
}

TEST_F(HexDumpTest, MultiLine) {
  const unsigned char buffer[33] = "abcdefgh12345678ABCDEFGH12345678";
  EXPECT_EQ(" a  b  c  d  e  f  g  h 31 32 33 34 35 36 37 38\n A  B  C  D  E  F  G  H 31 32 33 34 35 36 37 38\n",
            hexdump(buffer, 32, 0, true));
}

class UtilsTests: public ::testing::Test {
protected:
  virtual void SetUp() {
  }
};

TEST_F(UtilsTests, SplitStringWithEmpty) {
std::vector<string> exp;
std::string tcase;

exp = {"val1", "val2"};
EXPECT_THAT(exp, ContainerEq(split_string("val1;val2", ';')));

exp = {"", "val1", "val2"};
EXPECT_THAT(exp, ContainerEq(split_string(";val1;val2", ';')));

exp = {"val1", "val2", ""};
EXPECT_THAT(exp, ContainerEq(split_string("val1;val2;", ';')));

exp = {};
EXPECT_THAT(exp, ContainerEq(split_string("", ';')));

exp = {"", ""};
EXPECT_THAT(exp, ContainerEq(split_string(";", ';')));

// No trimming
exp = {"  val1", "val2  "};
EXPECT_THAT(exp, ContainerEq(split_string("  val1&val2  ", '&')));

}

TEST_F(UtilsTests, SplitStringWithoutEmpty) {
std::vector<string> exp;
std::string tcase;

exp = {"val1", "val2"};
EXPECT_THAT(exp, ContainerEq(split_string("val1;val2", ';', false)));

exp = {"val1", "val2"};
EXPECT_THAT(exp, ContainerEq(split_string(";val1;val2", ';', false)));

exp = {"val1", "val2"};
EXPECT_THAT(exp, ContainerEq(split_string("val1;val2;", ';', false)));

exp = {};
EXPECT_THAT(exp, ContainerEq(split_string("", ';', false)));

exp = {};
EXPECT_THAT(exp, ContainerEq(split_string(";", ';', false)));

// No trimming
exp = {"  val1", "val2  "};
EXPECT_THAT(exp, ContainerEq(split_string("  val1&val2  ", '&', false)));
}
