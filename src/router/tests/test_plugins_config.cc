/*
  Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "gtest_consoleoutput.h"
#include "cmd_exec.h"
#include "router_app.h"

#include <fstream>

#include "gmock/gmock.h"

using ::testing::HasSubstr;
using ::testing::StrEq;
using mysql_harness::Path;
using std::string;

string g_cwd;
Path g_origin;

class PluginsConfigTest : public ConsoleOutputTest {
protected:
  virtual void SetUp() {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(g_cwd));
    config_path->append("PluginsConfigTest.conf");
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

TEST_F(PluginsConfigTest, NoPluginLoaded) {
  reset_config();

  string cmd = app_mysqlrouter->str() + " -c " + config_path->str();
  auto cmd_result = cmd_exec(cmd, true);

  ASSERT_THAT(cmd_result.output, HasSubstr("MySQL Router not configured to load or start any plugin. Exiting."));
}

TEST_F(PluginsConfigTest, OnlyLoggerLoaded) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[logger]\n";
  c << "library = logger\n\n";
  c.close();

  string cmd = app_mysqlrouter->str() + " -c " + config_path->str();
  auto cmd_result = cmd_exec(cmd, true);

  ASSERT_THAT(cmd_result.output, HasSubstr("MySQL Router not configured to load or start any plugin. Exiting."));
}

TEST_F(PluginsConfigTest, TwoMetadadaCacheSections) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[logger]\n\n";
  c << "[metadata_cache:one]\n\n";
  c << "[metadata_cache:two]\n\n";
  c.close();

  string cmd = app_mysqlrouter->str() + " -c " + config_path->str();
  auto cmd_result = cmd_exec(cmd, true);

  ASSERT_THAT(cmd_result.output, HasSubstr("MySQL Router currently supports only one metadata_cache instance."));
}

TEST_F(PluginsConfigTest, SingleMetadataChacheSection) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[logger]\n\n";
  c << "[metadata_cache:one]\n\n";
  c.close();

  string cmd = app_mysqlrouter->str() + " -c " + config_path->str();
  auto cmd_result = cmd_exec(cmd, true);

  // shoule be ok but complain about missing user option
  ASSERT_THAT(cmd_result.output, HasSubstr("option user in [metadata_cache:one] is required"));
}

int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
