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

#include "utils.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <exception>
#include <unistd.h>

using ::testing::ContainerEq;
using ::testing::Pair;

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
