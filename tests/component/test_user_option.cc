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
};

// --user option is not supported on Windows
#ifndef _WIN32

// check that using --user with no sudo gives a proper error
TEST_F(RouterUserOptionTest, UserOptionNoSudo) {
  auto router = launch_router("--bootstrap=127.0.0.1:5000 --user=mysqlrouter");

  ASSERT_TRUE(router.expect_output("Error: One can only use the -u/--user switch if running as root"));
  ASSERT_EQ(router.wait_for_exit(), 1);

  // That's more to test the framework itself:
  // The consecutive calls to exit_code() should be possible  and return the same value
  ASSERT_EQ(router.exit_code(), 1);
  ASSERT_EQ(router.exit_code(), 1);
}

// check that using --user parameter before --bootstrap gives a proper error
TEST_F(RouterUserOptionTest, UserOptionBeforeBootstrap) {
  auto router = launch_router("--user=mysqlrouter --bootstrap=127.0.0.1:5000");

  ASSERT_TRUE(router.expect_output("Error: Option -u/--user needs to be used after the --bootstrap option"));
  ASSERT_EQ(router.wait_for_exit(), 1);
}

#else
// check that it really is not supported on Windows
TEST_F(RouterUserOptionTest, UserOptionOnWindows) {
  auto router = launch_router("--bootstrap=127.0.0.1:5000 --user=mysqlrouter");

  ASSERT_TRUE(router.expect_output("Error: unknown option '--user'.")
    ) << router.get_full_output();
  ASSERT_EQ(router.wait_for_exit(), 1);
}
#endif

int main(int argc, char *argv[]) {
   g_origin_path = Path(argv[0]).dirname();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
