/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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
 * BUG22020088
 *
 */

#include "gtest_consoleoutput.h"
#include "cmd_exec.h"
#include "../../../router/src/router_app.h"
#include "config_parser.h"
#include "plugin.h"

#include <fstream>
#include <memory>
#include <future>
#include <string>
#include <thread>

#include "gmock/gmock.h"

using std::string;
using ::testing::StrEq;

string g_cwd;
Path g_origin;

// Used in tests; does not change for each test.
const string kDefaultRoutingConfig = "\ndestinations=127.0.0.1:3306\nmode=read-only\n";

class Bug22020088 : public ConsoleOutputTest {
protected:
  virtual void SetUp() {
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(g_cwd));
    config_path->append("Bug21771595.ini");

  }

  void reset_config() {
    std::ofstream ofs_config(config_path->str());
    if (ofs_config.good()) {
      ofs_config << "[DEFAULT]\n";
      ofs_config << "logging_folder =\n";
      ofs_config << "plugin_folder = " << plugin_dir->str() << "\n";
      ofs_config << "runtime_folder = " << stage_dir->str() << "\n";
      ofs_config << "config_folder = " << stage_dir->str() << "\n\n";
      ofs_config.close();
    }
  }

  std::unique_ptr<Path> config_path;
};

TEST_F(Bug22020088, MissingBindAddressAndDefaultPort) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\n";
  c << kDefaultRoutingConfig;
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), StrEq(
      "in [routing]: either bind_port or bind_address is required"));
  }
}

TEST_F(Bug22020088, MissingPortInBindAddress) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nbind_address=127.0.0.1\n";
  c << kDefaultRoutingConfig;
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), StrEq(
     "in [routing]: no bind_port, and TCP port in bind_address is not valid"));
  }
}

TEST_F(Bug22020088, InvalidPortInBindAddress) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nbind_address=127.0.0.1:999292\n";
  c << kDefaultRoutingConfig;
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), StrEq(
     "option bind_address in [routing] is incorrect (invalid TCP port: invalid characters or too long)"));
  }
}

TEST_F(Bug22020088, InvalidDefaultPort) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nbind_port=23123124123123\n";
  c << kDefaultRoutingConfig;
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), StrEq(
     "option bind_port in [routing] needs value between 1 and 65535 inclusive, was '23123124123123'"));
  }
}


int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}