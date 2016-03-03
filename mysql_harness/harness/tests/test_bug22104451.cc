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
 * BUG22104451 Router hangs when config value length > 256 characters
 *
 */

#include "config_parser.h"

////////////////////////////////////////
// Third-party include files
#include "gtest/gtest.h"

class Bug22104451 : public ::testing::Test {
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(Bug22104451, ReadLongValues) {
  std::stringstream c;
  std:: string long_destinations = "localhost:13005,localhost:13003,"
    "localhost:13004,localhost:17001,localhost:17001,localhost:17001,"
    "localhost:17001,localhost:17001,localhost:17001,localhost:17001,"
    "localhost:17001,localhost:17001,localhost:17001,localhost:17001,"
    "localhost:17001,localhost:17001,localhost:17001,localhost:17001,"
    "localhost:17001,localhost:17001";

  c << "[routing:c]\n"
    << "bind_address = 127.0.0.1:7006\n"
    << "destinations = " << long_destinations << "\n"
    << "mode = read-only\n";

  EXPECT_NO_THROW({
    Config config(Config::allow_keys);
    std::istringstream input(c.str());
    config.read(input);
    EXPECT_EQ(long_destinations, config.get("routing", "c").get("destinations"));
  });
}
