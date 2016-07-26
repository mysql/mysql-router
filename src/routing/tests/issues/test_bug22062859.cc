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
 * BUG22062859 STARTING ROUTER FAILS IF THERE IS A SPACE IN DESTINATION ADDRESS
 *
 */

#include "config_parser.h"
#include "plugin_config.h"
#include "router_test_helpers.h"

#ifdef _WIN32
#include <WinSock2.h>
#endif
#include "gmock/gmock.h"

class Bug22062859 : public ::testing::Test {
  virtual void SetUp() { }

  virtual void TearDown() { }
};

TEST_F(Bug22062859, IgnoreSpacesInDestinations) {
  std::stringstream c;

  c << "[routing:c]\n"
  << "bind_address = 127.0.0.1:7006\n"
  << "destinations = localhost:13005,localhost:13003, localhost:13004"
  << ",   localhost:1300,   localhost  ,localhost , localhost         \n"
  << "mode = read-only\n";

  mysql_harness::Config config(mysql_harness::Config::allow_keys);
  std::istringstream input(c.str());
  config.read(input);

  EXPECT_NO_THROW(
      {
        mysql_harness::ConfigSection &section = config.get("routing", "c");
        RoutingPluginConfig rconfig(&section);
      }
  );
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
