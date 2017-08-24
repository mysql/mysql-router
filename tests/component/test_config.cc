/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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
#include "router_component_test.h"

Path g_origin_path;

class RouterConfigTest : public RouterComponentTest, public ::testing::Test {
 protected:
  virtual void SetUp() {
    set_origin(g_origin_path);
    RouterComponentTest::SetUp();
  }

  TcpPortPool port_pool_;
};

// Bug #25800863 WRONG ERRORMSG IF DIRECTORY IS PROVIDED AS CONFIGFILE
TEST_F(RouterConfigTest, RoutingDirAsMainConfigDirectory) {
  const std::string config_dir = get_tmp_dir();

  // launch the router giving directory instead of config_name
  auto router = launch_router("-c " +  config_dir);

  EXPECT_TRUE(router.expect_output(
    "Expected configuration file, got directory name: " + config_dir)
  ) << "router output: "<< router.get_full_output() << std::endl;

  EXPECT_EQ(router.wait_for_exit(), 1);
}

// Bug #25800863 WRONG ERRORMSG IF DIRECTORY IS PROVIDED AS CONFIGFILE
TEST_F(RouterConfigTest, RoutingDirAsExtendedConfigDirectory) {
  const auto router_port = port_pool_.get_next_available();
  const auto server_port = port_pool_.get_next_available();

  const std::string routing_section =
                      "[routing:basic]\n"
                      "bind_port = " + std::to_string(router_port) + "\n"
                      "mode = read-write\n"
                      "destinations = 127.0.0.1:" + std::to_string(server_port) + "\n";

  std::string conf_file = create_config_file(routing_section);
  const std::string config_dir = get_tmp_dir();

  // launch the router giving directory instead of an extra config name
  auto router = launch_router("-c " +  conf_file + " -a " + config_dir);

  EXPECT_TRUE(router.expect_output(
    "Expected configuration file, got directory name: " + config_dir)
  ) << "router output: "<< router.get_full_output() << std::endl;

  EXPECT_EQ(router.wait_for_exit(), 1);
}


int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
