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

#include "config_parser.h"
#include "plugin_config.h"
#include "router_test_helpers.h"

#include "gmock/gmock.h"

using ::testing::StrEq;

// <destinations string, expected exception message>:
using DestException = std::pair<const char*, const char*>;

class DestinationsFabric : public ::testing::TestWithParam<DestException> {
 public:
  void create_routing_config(const std::string &destinations,
    const std::string &exception_msg);
};


TEST_P(DestinationsFabric, CreateRoutingConfig)
{
  const char* destinations = GetParam().first;
  const char* exception_msg = GetParam().second;

  std::string conf_str =
    "[routing:modeReadWrite]\n"
    "bind_port = 7001\n"
    "destinations = " + std::string(destinations) + "\n"
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
    ASSERT_STREQ(err.what(), exception_msg);
  }
}

INSTANTIATE_TEST_CASE_P(FabricCacheEmptyCommand, DestinationsFabric,
  ::testing::Values (
    std::make_pair("fabric+cache:",
        "option destinations in [routing:modeReadWrite] has an invalid destination"
        " address 'fabric+cache:3306'"),

    std::make_pair("fabric+cache:/",
      "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache://",
      "option destinations in [routing:modeReadWrite] has an invalid Fabric"
       " command in URI; was ''"),

    std::make_pair("fabric+cache:///",
      "option destinations in [routing:modeReadWrite] has an invalid Fabric"
      " command in URI; was ''"),

    std::make_pair("fabric+cache:////",
      "option destinations in [routing:modeReadWrite] has an invalid Fabric"
      " command in URI; was ''")
  )
);

INSTANTIATE_TEST_CASE_P(FabricCacheInvalidQuery, DestinationsFabric,
  ::testing::Values(
    std::make_pair("fabric+cache:///group/my_group1?al",
      "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache:///group/my_group1?al=",
      "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache:///group/my_group1?",
       "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache:///group/my_group1??",
        "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache:///group/my_group1?=?",
        "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache:///group/my_group1?al",
        "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache:///group/?al",
        "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache:///group/my_group1?\?=&",
        "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache:///group/my_group1?&",
        "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache:///group/my_group1?&=",
        "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache:///group/my_group1?&==",
        "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache:///group/my_group1?&&",
        "invalid TCP port: invalid characters or too long"),

    std::make_pair("fabric+cache:///group/my_group1??&",
        "invalid TCP port: invalid characters or too long")
  )
);

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

