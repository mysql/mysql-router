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

class RouterUserOptionTest : public RouterComponentTest, public ::testing::Test {
 protected:
  virtual void SetUp() {
    set_origin(g_origin_path);
    RouterComponentTest::SetUp();
  }

  TcpPortPool port_pool_;
};

TEST_F(RouterUserOptionTest, BootstrapOk) {
  const std::string json_stmts = get_data_dir().join("bootstrapper.json").str();
  const std::string bootstrap_dir = get_tmp_dir();

  const auto server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port);
  bool ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(ready) << "Timed out waiting for mock server port ready" << std::endl
                     << server_mock.get_full_output();

  // launch the router in bootstrap mode
  std::shared_ptr<void> exit_guard(nullptr, [&](void*){purge_dir(bootstrap_dir);});
  auto router = launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port)
                              + " -d " + bootstrap_dir);

  // add login hook
  router.register_response("Please enter MySQL password for root: ", "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output("MySQL Router  has now been configured for the InnoDB cluster 'test'")
    ) << router.get_full_output() << std::endl << "server: " << server_mock.get_full_output();
  EXPECT_EQ(router.wait_for_exit(), 0);
}

TEST_F(RouterUserOptionTest, BootstrapOnlySockets) {
  const std::string json_stmts = get_data_dir().join("bootstrapper.json").str();
  const std::string bootstrap_dir = get_tmp_dir();
  const auto server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port);
  bool ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(ready) << server_mock.get_full_output();

  // do the bootstrap, request using only sockets
  std::shared_ptr<void> exit_guard(nullptr, [&](void*){purge_dir(bootstrap_dir);});
  auto router = launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port)
                              + " -d " + bootstrap_dir
                              + " --conf-skip-tcp"
                              + " --conf-use-sockets");

  // add login hook
  router.register_response("Please enter MySQL password for root: ", "fake-pass\n");

#ifndef _WIN32
  // check if bootstrap output contains sockets info
  EXPECT_TRUE(router.expect_output("Read/Write Connections: .*/mysqlx.sock", true /*regex*/)
              && router.expect_output("Read/Only Connections: .*/mysqlxro.sock", true /*regex*/)
    ) << "router: " << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  EXPECT_EQ(router.wait_for_exit(), 0);
#else
  // on Windows Unix socket functionality in not available
  EXPECT_TRUE(router.expect_output("Error: unknown option '--conf-skip-tcp'")
    ) << "router: " << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  EXPECT_EQ(router.wait_for_exit(), 1);
#endif

}

TEST_F(RouterUserOptionTest, BootstrapUnsupportedSchemaVersion) {
  const std::string json_stmts = get_data_dir().join("bootstrap_usupported_schema_version.json").str();
  const std::string bootstrap_dir = get_tmp_dir();
  const auto server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port);
  bool ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(ready) << server_mock.get_full_output();

  // launch the router in bootstrap mode
  std::shared_ptr<void> exit_guard(nullptr, [&](void*){purge_dir(bootstrap_dir);});
  auto router = launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port)
                              + " -d " + bootstrap_dir);

  router.register_response("Please enter MySQL password for root: ", "fake-pass\n");

  // check that it failed as expected
  EXPECT_TRUE(router.expect_output("This version of MySQL Router is not compatible with the provided MySQL InnoDB cluster metadata")
    ) << "router: " << router.get_full_output()  << std::endl
      << "server: " << server_mock.get_full_output();
  EXPECT_EQ(router.wait_for_exit(), 1);
}

TEST_F(RouterUserOptionTest, BootstrapSucceedWhenServerResponseLessThanReadTimeout) {
  const std::string json_stmts = get_data_dir().join("bootstrap_exec_time_2_seconds.json").str();
  const std::string bootstrap_dir = get_tmp_dir();

  const auto server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port);
  bool ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(ready) << "Timed out waiting for mock server port ready" << std::endl
                     << server_mock.get_full_output();

  // launch the router in bootstrap mode
  std::shared_ptr<void> exit_guard(nullptr, [&](void*){purge_dir(bootstrap_dir);});
  auto router = launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port)
                              + " -d " + bootstrap_dir + " --connect-timeout=3 --read-timeout=3");

  // add login hook
  router.register_response("Please enter MySQL password for root: ", "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output("MySQL Router  has now been configured for the InnoDB cluster 'test'", false, 3000)
    ) << router.get_full_output() << std::endl << "server: " << server_mock.get_full_output();
  EXPECT_EQ(router.wait_for_exit(), 0);
}

TEST_F(RouterUserOptionTest, BootstrapFailWhenServerResponseExceedsReadTimeout) {
  const std::string json_stmts = get_data_dir().join("bootstrap_exec_time_2_seconds.json").str();
  const std::string bootstrap_dir = get_tmp_dir();

  const auto server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port);
  bool ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(ready) << "Timed out waiting for mock server port ready" << std::endl
                     << server_mock.get_full_output();

  // launch the router in bootstrap mode
  std::shared_ptr<void> exit_guard(nullptr, [&](void*){purge_dir(bootstrap_dir);});
  auto router = launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port)
                              + " -d " + bootstrap_dir + " --connect-timeout=1 --read-timeout=1");

  // add login hook
  router.register_response("Please enter MySQL password for root: ", "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output("Error: Error executing MySQL query: Lost connection to MySQL server during query (2013)", false, 3000)
    ) << router.get_full_output() << std::endl << "server: " << server_mock.get_full_output();
  EXPECT_EQ(router.wait_for_exit(), 1);
}


int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
