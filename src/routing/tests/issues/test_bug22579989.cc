/*
  Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin_config.h"
#include "config_parser.h"

#include "gmock/gmock.h"

class Bug22579989 : public ::testing::Test {
protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  Config get_routing_config(std::string destinations) {
    std::stringstream c;

    c << "[routing:c]\n"
      << "bind_address = 127.0.0.1:7006\n"
      << "mode = read-only\n"
      << "destinations = "
      << destinations << "\n\n";

    Config config(Config::allow_keys);
    std::istringstream input(c.str());
    config.read(input);

    return config;
  }
};

TEST_F(Bug22579989, EmptyValuesInCSVCase1) {
  std::stringstream c;
  std::string destinations = "localhost:13005,localhost:13003,localhost:13004,";

  Config config = get_routing_config(destinations);

  EXPECT_THROW({
    ConfigSection& section = config.get("routing", "c");
    RoutingPluginConfig rconfig(&section);
  }, std::invalid_argument);
}

TEST_F(Bug22579989, EmptyValuesInCSVCase2) {
  std::stringstream c;
  std::string destinations = "localhost:13005,localhost:13003,localhost:13004, , ,";

  Config config = get_routing_config(destinations);

  EXPECT_THROW({
    ConfigSection& section = config.get("routing", "c");
    RoutingPluginConfig rconfig(&section);
  }, std::invalid_argument);
}

TEST_F(Bug22579989, EmptyValuesInCSVCase3) {
  std::stringstream c;
  std::string destinations = "localhost:13005, ,,localhost:13003,localhost:13004";

  Config config = get_routing_config(destinations);

  EXPECT_THROW({
    ConfigSection& section = config.get("routing", "c");
    RoutingPluginConfig rconfig(&section);
  }, std::invalid_argument);
}

TEST_F(Bug22579989, EmptyValuesInCSVCase4) {
  std::stringstream c;
  std::string destinations = ",localhost:13005,localhost:13003,localhost:13004";

  Config config = get_routing_config(destinations);

  EXPECT_THROW({
    ConfigSection& section = config.get("routing", "c");
    RoutingPluginConfig rconfig(&section);
  }, std::invalid_argument);
}

TEST_F(Bug22579989, EmptyValuesInCSVCase5) {
  std::stringstream c;
  std::string destinations = ",, ,";

  Config config = get_routing_config(destinations);

  EXPECT_THROW({
    ConfigSection& section = config.get("routing", "c");
    RoutingPluginConfig rconfig(&section);
  }, std::invalid_argument);
}

TEST_F(Bug22579989, EmptyValuesInCSVCase6) {
  std::stringstream c;
  std::string destinations = ",localhost:13005, ,,localhost:13003,localhost:13004, ,";

  Config config = get_routing_config(destinations);

  EXPECT_THROW({
    ConfigSection& section = config.get("routing", "c");
    RoutingPluginConfig rconfig(&section);
  }, std::invalid_argument);
}

TEST_F(Bug22579989, NoEmptyValuesInCSV) {
  std::stringstream c;
  std::string destinations = "localhost:13005,localhost:13003,localhost:13004";

  Config config = get_routing_config(destinations);

  EXPECT_NO_THROW({
    ConfigSection& section = config.get("routing", "c");
    RoutingPluginConfig rconfig(&section);
  });
}