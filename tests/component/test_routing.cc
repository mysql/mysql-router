/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "router_config.h"  // defines HAVE_PRLIMIT
#ifdef HAVE_PRLIMIT
  #include <sys/resource.h> // prlimit()
#endif
#include "gmock/gmock.h"
#include "router_component_test.h"
#include "router_test_helpers.h"
#include "mysql_session.h"

#include <thread>
#include <chrono>

Path g_origin_path;

class RouterRoutingTest : public RouterComponentTest, public ::testing::Test {
 protected:
  virtual void SetUp() {
    set_origin(g_origin_path);
    RouterComponentTest::SetUp();
  }

  TcpPortPool port_pool_;
};

TEST_F(RouterRoutingTest, RoutingOk) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // use the json file that adds additional rows to the metadata to increase the
  // packet size to +10MB to verify routing of the big packets
  const std::string json_stmts = get_data_dir().join("bootstrap_big_data.json").str();
  const std::string bootstrap_dir = get_tmp_dir();

  // launch the server mock for bootstrapping
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port,
                                              false /*expecting huge data, can't print on the console*/);

  const std::string routing_section =
                      "[routing:basic]\n"
                      "bind_port = " + std::to_string(router_port) + "\n"
                      "mode = read-write\n"
                      "destinations = 127.0.0.1:" + std::to_string(server_port) + "\n";

  std::string conf_file = create_config_file(routing_section);


  // launch the router with simple static routing configuration
  auto router_static = launch_router("-c " +  conf_file);

  // wait for both to begin accepting the connections
  ASSERT_TRUE(wait_for_port_ready(server_port, 5000))
    << server_mock.get_full_output();

  ASSERT_TRUE(wait_for_port_ready(router_port, 5000))
    << router_static.get_full_output();

  // launch another router to do the bootstrap connecting to the mock server
  // via first router instance
  std::shared_ptr<void> exit_guard(nullptr, [&](void*){purge_dir(bootstrap_dir);});
  auto router_bootstrapping = launch_router("--bootstrap=localhost:" + std::to_string(router_port)
                                            + " -d " +  bootstrap_dir);

  router_bootstrapping.register_response("Please enter MySQL password for root: ", "fake-pass\n");

  ASSERT_EQ(router_bootstrapping.wait_for_exit(), 0
  ) << "bootstrap output: " << router_bootstrapping.get_full_output() << std::endl
    << "routing output: "<< router_static.get_full_output() << std::endl
    << "server output: "<< server_mock.get_full_output() << std::endl;

  ASSERT_TRUE(router_bootstrapping.expect_output(
    "MySQL Router  has now been configured for the InnoDB cluster 'test'")
  ) << "bootstrap output: " << router_bootstrapping.get_full_output() << std::endl
    << "routing output: "<< router_static.get_full_output() << std::endl
    << "server output: "<< server_mock.get_full_output() << std::endl;
}

TEST_F(RouterRoutingTest, RoutingTooManyConnections) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // doesn't really matter which file we use here, we are not going to do any queries
  const std::string json_stmts = get_data_dir().join("bootstrap_big_data.json").str();

  // launch the server mock
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false);

  // create a config with routing that has max_connections == 2
  const std::string routing_section =
                      "[routing:basic]\n"
                      "bind_port = " + std::to_string(router_port) + "\n"
                      "mode = read-write\n"
                      "max_connections = 2\n"
                      "destinations = 127.0.0.1:" + std::to_string(server_port) + "\n";

  std::string conf_file = create_config_file(routing_section);

  // launch the router with the created configuration
  auto router_static = launch_router("-c " +  conf_file);

  // wait for server and router to begin accepting the connections
  ASSERT_TRUE(wait_for_port_ready(server_port, 5000)) << server_mock.get_full_output();
  ASSERT_TRUE(wait_for_port_ready(router_port, 5000)) << router_static.get_full_output();

  // try to create 3 connections, the third should fail
  // because of the max_connections limit being exceeded
  mysqlrouter::MySQLSession client1, client2, client3;
  EXPECT_NO_THROW(client1.connect("127.0.0.1", router_port, "username", "password", "", ""));
  EXPECT_NO_THROW(client2.connect("127.0.0.1", router_port, "username", "password", "", ""));
  ASSERT_THROW_LIKE(
    client3.connect("127.0.0.1", router_port, "username", "password", "", ""),
    std::runtime_error,
    "Too many connections to MySQL Router (1040)"
  );
}

// this test uses OS-specific methods to restrict thread creation
#ifdef HAVE_PRLIMIT
TEST_F(RouterRoutingTest, RoutingPluginCantSpawnMoreThreads) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // doesn't really matter which file we use here, we are not going to do any queries
  const std::string json_stmts = get_data_dir().join("bootstrap_big_data.json").str();

  // launch the server mock
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false);

  // create a basic config
  const std::string routing_section =
                      "[routing:basic]\n"
                      "bind_port = " + std::to_string(router_port) + "\n"
                      "mode = read-write\n"
                      "destinations = 127.0.0.1:" + std::to_string(server_port) + "\n";

  std::string conf_file = create_config_file(routing_section);

  // launch the router with the created configuration
  auto router_static = launch_router("-c " +  conf_file);

  // wait for server and router to begin accepting the connections
  ASSERT_TRUE(wait_for_port_ready(server_port, 5000)) << server_mock.get_full_output();
  ASSERT_TRUE(wait_for_port_ready(router_port, 5000)) << router_static.get_full_output();

  // don't allow router to create any more (client) threads
  {
    pid_t pid = router_static.get_pid();

    // how many threads Router process is allowed to have. If this number is lower
    // than current count, nothing will happen, but new ones will not be allowed
    // to be created until count comes down below this limit.
    // Thus 0 is a nice number to ensure nothing gets spawned anymore.
    rlim_t max_threads = 0;

    struct rlimit new_limit {
      .rlim_cur = max_threads,
      .rlim_max = max_threads
    };
    EXPECT_EQ(0, prlimit(pid, RLIMIT_NPROC, &new_limit, nullptr));
  }

  // try to create a new connection - it should fail because std::thread() in
  // routing plugin will fail to spawn a new thread for this new connection
  mysqlrouter::MySQLSession client1;
  ASSERT_THROW_LIKE(
    client1.connect("127.0.0.1", router_port, "username", "password", "", ""),
    std::runtime_error,
    "Router couldn't spawn a new thread to service new client connection (1040)"
  );
}
#endif // #ifndef _WIN32

#ifndef _WIN32  // named sockets are not supported on Windows;
                // on Unix, they're implemented using Unix sockets
TEST_F(RouterRoutingTest, named_socket_has_right_permissions) {
  /**
   * @test Verify that unix socket has the required file permissions so that it
   *       can be connected to by all users. According to man 7 unix, only r+w
   *       permissions are required, but Server sets x as well, so we do the same.
   */

  // get config dir (we will also stuff our unix socket file there)
  const std::string bootstrap_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr, [&](void*){purge_dir(bootstrap_dir);});

  // launch Router with unix socket
  const std::string socket_file = bootstrap_dir + "/sockfile";
  const std::string routing_section =
                      "[routing:basic]\n"
                      "socket = " + socket_file + "\n"
                      "mode = read-write\n"
                      "destinations = 127.0.0.1:1234\n";  // port can be bogus
  const std::string conf_file = create_config_file(routing_section);
  auto router_static = launch_router("-c " +  conf_file);

  // loop until socket file appears and has correct permissions
  auto wait_for_correct_perms = [&socket_file](int timeout_ms) {
    const mode_t expected_mode = S_IFSOCK |
                                 S_IRUSR | S_IWUSR | S_IXUSR |
                                 S_IRGRP | S_IWGRP | S_IXGRP |
                                 S_IROTH | S_IWOTH | S_IXOTH;
    while (timeout_ms > 0) {
      struct stat info;

      memset(&info, 0, sizeof(info));
      stat(socket_file.c_str(), &info); // silently ignore error when file doesn't exist yet

      if (info.st_mode == expected_mode)
        return true;

      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      timeout_ms -= 10;
    }

    return false;
  };

  EXPECT_THAT(wait_for_correct_perms(5000), testing::Eq(true));
}
#endif

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
