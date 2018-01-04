/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _WIN32  // named sockets are not supported on Windows;
                // on Unix, they're implemented using Unix sockets

#include "gmock/gmock.h"

#include "../../routing/src/mysql_routing.h"
#include "router_test_helpers.h"
#include "test/helpers.h"

class TestSetupNamedSocketService : public ::testing::Test {};

TEST_F(TestSetupNamedSocketService, unix_socket_permissions_failure) {
  /**
   * @test Verify that failure while setting unix socket permissions throws correctly
   */

  EXPECT_THROW_LIKE(
    MySQLRouting::set_unix_socket_permissions("/no/such/file"),
    std::runtime_error,
    "Failed setting file permissions on socket file '/no/such/file': No such file or directory"
  );
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  init_test_logger();
  return RUN_ALL_TESTS();
}

#endif // #ifndef _WIN32
