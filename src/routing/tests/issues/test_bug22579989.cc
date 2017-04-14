/*
  Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
 * BUG22579989 Fix reporting empty values in destinations given as CSV
 *
 */

#include "mysql/harness/config_parser.h"
#include "plugin_config.h"
#include "router_test_helpers.h"

#ifdef _WIN32
#include <WinSock2.h>
#endif

#include "gmock/gmock.h"

class Bug22579989 : public ::testing::Test {
 protected:
  virtual void SetUp() { }

  virtual void TearDown() { }

  mysql_harness::Config get_routing_config(std::string destinations) {
    std::stringstream c;

    c << "[routing:c]\n"
    << "bind_address = 127.0.0.1:7006\n"
    << "mode = read-only\n"
    << "destinations = "
    << destinations << "\n\n";

    mysql_harness::Config config(mysql_harness::Config::allow_keys);
    std::istringstream input(c.str());
    config.read(input);

    return config;
  }
};

TEST_F(Bug22579989, EmptyValuesInCSVCase1) {
  std::stringstream c;
  std::string destinations = "localhost:13005,localhost:13003,localhost:13004,";

  mysql_harness::Config config = get_routing_config(destinations);

  EXPECT_THROW(
      {
        mysql_harness::ConfigSection &section = config.get("routing", "c");
        RoutingPluginConfig rconfig(&section);
      }, std::invalid_argument);
}

TEST_F(Bug22579989, EmptyValuesInCSVCase2) {
  std::stringstream c;
  std::string destinations = "localhost:13005,localhost:13003,localhost:13004, , ,";

  mysql_harness::Config config = get_routing_config(destinations);

  EXPECT_THROW(
      {
        mysql_harness::ConfigSection &section = config.get("routing", "c");
        RoutingPluginConfig rconfig(&section);
      }, std::invalid_argument);
}

TEST_F(Bug22579989, EmptyValuesInCSVCase3) {
  std::stringstream c;
  std::string destinations = "localhost:13005, ,,localhost:13003,localhost:13004";

  mysql_harness::Config config = get_routing_config(destinations);

  EXPECT_THROW(
      {
        mysql_harness::ConfigSection &section = config.get("routing", "c");
        RoutingPluginConfig rconfig(&section);
      }, std::invalid_argument);
}

TEST_F(Bug22579989, EmptyValuesInCSVCase4) {
  std::stringstream c;
  std::string destinations = ",localhost:13005,localhost:13003,localhost:13004";

  mysql_harness::Config config = get_routing_config(destinations);

  EXPECT_THROW(
      {
        mysql_harness::ConfigSection &section = config.get("routing", "c");
        RoutingPluginConfig rconfig(&section);
      }, std::invalid_argument);
}

TEST_F(Bug22579989, EmptyValuesInCSVCase5) {
  std::stringstream c;
  std::string destinations = ",, ,";

  mysql_harness::Config config = get_routing_config(destinations);

  EXPECT_THROW(
      {
        mysql_harness::ConfigSection &section = config.get("routing", "c");
        RoutingPluginConfig rconfig(&section);
      }, std::invalid_argument);
}

TEST_F(Bug22579989, EmptyValuesInCSVCase6) {
  std::stringstream c;
  std::string destinations = ",localhost:13005, ,,localhost:13003,localhost:13004, ,";

  mysql_harness::Config config = get_routing_config(destinations);

  EXPECT_THROW(
      {
        mysql_harness::ConfigSection &section = config.get("routing", "c");
        RoutingPluginConfig rconfig(&section);
      }, std::invalid_argument);
}

TEST_F(Bug22579989, NoEmptyValuesInCSV) {
  std::stringstream c;
  std::string destinations = "localhost:13005,localhost:13003,localhost:13004";

  mysql_harness::Config config = get_routing_config(destinations);

  EXPECT_NO_THROW(
      {
        mysql_harness::ConfigSection &section = config.get("routing", "c");
        RoutingPluginConfig rconfig(&section);
      });
}


int main(int argc, char *argv[]) {
  init_windows_sockets();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
