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

#include "utilities.h"

////////////////////////////////////////
// Third-party include files
#include "gtest/gtest.h"

////////////////////////////////////////
// Standard include files
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

using std::map;
using std::string;
using std::pair;
using std::make_pair;

TEST(TestUtilities, Strip) {
  const char *strings[][2] = {
    { "foo", "foo", },
    { " foo", "foo", },
    { "foo ", "foo", },
    { " \tfoo \t\t", "foo", },
    { "", "" },
  };

  for (auto sample: make_range(strings, sizeof(strings)/sizeof(*strings)))
  {
    std::string str(sample[0]);
    strip(str);
    EXPECT_EQ(sample[1], str);
  }
}

TEST(TestUtilities, FindRangeFirst) {
  typedef map<pair<string, string>, string> Map;
  Map assoc;
  assoc.emplace(make_pair("one", "first"), "alpha");
  assoc.emplace(make_pair("one", "second"), "beta");
  assoc.emplace(make_pair("two", "first"), "gamma");
  assoc.emplace(make_pair("two", "second"), "delta");
  assoc.emplace(make_pair("two", "three"), "epsilon");

  auto rng1 = find_range_first(assoc, "one");
  ASSERT_NE(rng1.first, assoc.end());
  EXPECT_NE(rng1.second, assoc.end());
  EXPECT_EQ(2, distance(rng1.first, rng1.second));
  EXPECT_EQ("alpha", rng1.first++->second);
  EXPECT_EQ("beta", rng1.first++->second);
  EXPECT_EQ(rng1.second, rng1.first);

  auto rng2 = find_range_first(assoc, "two");
  ASSERT_NE(rng2.first, assoc.end());
  EXPECT_EQ(rng2.second, assoc.end());
  EXPECT_EQ(3, distance(rng2.first, rng2.second));
  EXPECT_EQ("gamma", rng2.first++->second);
  EXPECT_EQ("delta", rng2.first++->second);
  EXPECT_EQ("epsilon", rng2.first++->second);
  EXPECT_EQ(rng2.second, rng2.first);

  // Check for ranges that do not exist
  auto rng3 = find_range_first(assoc, "aardvark");
  EXPECT_EQ(0, distance(rng3.first, rng3.second));

  auto rng4 = find_range_first(assoc, "xyzzy");
  EXPECT_EQ(rng4.first, assoc.end());
  EXPECT_EQ(0, distance(rng4.first, rng4.second));
}
