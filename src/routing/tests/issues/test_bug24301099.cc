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
* BUG24301099 - SEGMENTATION FAULT WHEN FABRIC DESTINATION ADDR IS CONFIGURED WITH EMPTY STRINGS
*
*/


#include "config_parser.h"
#include "plugin_config.h"
#include "router_test_helpers.h"

#include "gmock/gmock.h"

using ::testing::StrEq;


class Bug24301099 : public ::testing::Test {
 public:
  void create_routing_config(const std::string &destinations,
    const std::string &exception_msg);

 private:
  virtual void SetUp() { }

  virtual void TearDown() { }
};

void Bug24301099::create_routing_config(const std::string &destinations,
                                        const std::string &exception_msg) {
  std::string conf_str =
    "[routing:modeReadWrite]\n"
    "bind_port = 7001\n"
    "destinations = " + destinations + "\n"
    "mode = read-write\n";

  mysql_harness::Config config(mysql_harness::Config::allow_keys);
  std::istringstream input(conf_str);
  config.read(input);

  mysql_harness::ConfigSection &section = config.get("routing",
                                                     "modeReadWrite");

  try {
    RoutingPluginConfig rconfig(&section);
  }
  catch (std::exception &err) {
    ASSERT_STREQ(err.what(), exception_msg.c_str());
  }
}

TEST_F(Bug24301099, FabricCacheEmptyCommand) {
  create_routing_config("fabric+cache:",
    "option destinations in [routing:modeReadWrite] has an invalid destination"
     " address 'fabric+cache:3306'");

  create_routing_config("fabric+cache:/",
    "invalid TCP port: invalid characters or too long");

  create_routing_config("fabric+cache://",
    "option destinations in [routing:modeReadWrite] has an invalid Fabric"
     " command in URI; was ''");

  create_routing_config("fabric+cache:///",
    "option destinations in [routing:modeReadWrite] has an invalid Fabric"
    " command in URI; was ''");

  create_routing_config("fabric+cache:////",
    "option destinations in [routing:modeReadWrite] has an invalid Fabric"
    " command in URI; was ''");
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
