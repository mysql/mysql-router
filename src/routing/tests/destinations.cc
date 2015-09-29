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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <functional>
#include <iostream>
#include <stdexcept>

#include "logger.h"
#include "destination.h"

#include "mysqlrouter/datatypes.h"

using mysqlrouter::TCPAddress;
using ::testing::StrEq;

class RouteDestinationTest : public ::testing::Test {
protected:
  virtual void SetUp() {
  }
};

TEST_F(RouteDestinationTest, Constructor)
{
  RouteDestination d;
  ASSERT_EQ(d.size(), 0);
}

TEST_F(RouteDestinationTest, Add)
{
  RouteDestination d;
  d.add("addr1", 1);
  ASSERT_EQ(d.size(), 1);
  d.add("addr2", 2);
  ASSERT_EQ(d.size(), 2);

  // Already added destination
  d.add("addr1", 1);
  ASSERT_EQ(d.size(), 2);
}

TEST_F(RouteDestinationTest, Remove)
{
  RouteDestination d;
  d.add("addr1", 1);
  d.add("addr99", 99);
  d.add("addr2", 2);
  ASSERT_EQ(d.size(), 3);
  d.remove("addr99", 99);
  ASSERT_EQ(d.size(), 2);
  d.remove("addr99", 99);
  ASSERT_EQ(d.size(), 2);
}

TEST_F(RouteDestinationTest, Get)
{
  RouteDestination d;
  ASSERT_THROW(d.get("addr1", 1), std::out_of_range);
  d.add("addr1", 1);
  ASSERT_NO_THROW(d.get("addr1", 1));

  TCPAddress addr = d.get("addr1", 1);
  ASSERT_THAT(addr.addr, StrEq("addr1"));
  EXPECT_EQ(addr.port, 1);

  d.remove("addr1", 1);
  ASSERT_THAT(addr.addr, StrEq("addr1"));
  EXPECT_EQ(addr.port, 1);
}

TEST_F(RouteDestinationTest, Size)
{
  RouteDestination d;
  ASSERT_EQ(d.size(), 0);
  d.add("addr1", 1);
  ASSERT_EQ(d.size(), 1);
  d.remove("addr1", 1);
  ASSERT_EQ(d.size(), 0);
}

TEST_F(RouteDestinationTest, RemoveAll)
{
  RouteDestination d;

  d.add("addr1", 1);
  d.add("addr2", 2);
  d.add("addr3", 3);
  ASSERT_EQ(d.size(), 3);

  d.clear();
  ASSERT_EQ(d.size(), 0);
}
