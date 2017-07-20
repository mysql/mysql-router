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

#include <fstream>
#include <functional>

using testing::HasSubstr;
Path g_origin_path;

class RouterLoggingTest : public RouterComponentTest, public ::testing::Test {
 protected:
  virtual void SetUp() {
    set_origin(g_origin_path);
    RouterComponentTest::SetUp();
  }

  bool find_in_log(const std::string logging_folder, const std::function<bool(const std::string&)>& predicate) {
    // This is proxy function to account for the fact that I/O can sometimes be slow.
    // If real_find_in_log() fails, it will retry 3 more times

    bool res = false;
    for (int retries_left = 3; retries_left; retries_left--) {
      try {
        res = real_find_in_log(logging_folder, predicate);
      } catch (const std::runtime_error&) {
        // report I/O error only on the last attempt
        if (retries_left == 1) {
          std::cerr << "  find_in_log() failed, giving up." << std::endl;
          throw;
        }
      }

      if (res)
        return true;
      if (retries_left) {
        std::cerr << "  find_in_log() failed, sleeping a bit and retrying..." << std::endl;
#ifdef _WIN32
        Sleep(5000);
#else
        sleep(5);
#endif
      }
    }

    return false;
  }

 private:
  bool real_find_in_log(const std::string logging_folder, const std::function<bool(const std::string&)>& predicate) {
    Path file(logging_folder + "/" + "mysqlrouter.log");
    std::ifstream ifs(file.c_str());
    if (!ifs)
      throw std::runtime_error("Error opening file " + file.str());

    std::string line;
    while (std::getline(ifs, line))
      if (predicate(line))
        return true;
    return false;
  }

};

TEST_F(RouterLoggingTest, log_startup_failure_to_console) {
  // This test verifies that fatal error message thrown in MySQLRouter::start()
  // during startup (before Loader::start() takes over) are properly logged to STDERR

  std::string conf_file = create_config_file("");

  // run the router and wait for it to exit
  auto router = launch_router("-c " +  conf_file);
  EXPECT_EQ(router.wait_for_exit(), 1);

  // expect something like this to appaer on STDERR
  // 2017-06-18 15:24:32 main ERROR [7ffff7fd4780] Error: MySQL Router not configured to load or start any plugin. Exiting.
  std::string out = router.get_full_output();
  EXPECT_THAT(out.c_str(), HasSubstr(" main ERROR "));
  EXPECT_THAT(out.c_str(), HasSubstr(" Error: MySQL Router not configured to load or start any plugin. Exiting."));
}

TEST_F(RouterLoggingTest, log_startup_failure_to_logfile) {
  // This test is the same as log_startup_failure_to_logfile(), but the failure
  // message is expected to be logged into a logfile

  // create tmp dir where we will log
  const std::string logging_folder = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr, [&](void*){purge_dir(logging_folder);});

  // create config with logging_folder set to that directory
  std::map<std::string, std::string> params = get_DEFAULT_defaults();
  params.at("logging_folder") = logging_folder;
  std::string conf_file = create_config_file("", &params);

  // run the router and wait for it to exit
  auto router = launch_router("-c " +  conf_file);
  EXPECT_EQ(router.wait_for_exit(), 1);

  // expect something like this to appear in log:
  // 2017-06-18 15:24:32 main ERROR [7ffff7fd4780] Error: MySQL Router not configured to load or start any plugin. Exiting.
  auto matcher = [](const std::string& line) -> bool {
    return line.find(" main ERROR ") != line.npos &&
           line.find(" Error: MySQL Router not configured to load or start any plugin. Exiting.") != line.npos;
  };
  EXPECT_TRUE(find_in_log(logging_folder, matcher));
}

int main(int argc, char *argv[]) {
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
