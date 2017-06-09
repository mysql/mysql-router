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

/**
 * BUG22572346 CORE DUMP WHILE STARTING THE ROUTER WHEN DESTINATIONS HAS @ CHARACTER
 *
 */
#include "gtest_consoleoutput.h"
#include "router_app.h"
#include "config_parser.h"
#include "router_test_helpers.h"

#include <fstream>
#include <string>

#include "gmock/gmock.h"

using std::string;
using ::testing::StrEq;
using mysql_harness::Path;

string g_cwd;
Path g_origin;

class Bug22572346 : public ConsoleOutputTest {
protected:
  virtual void SetUp() {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(g_cwd));
    config_path->append("Bug21572346.conf");
  }

  void reset_config() {
    std::ofstream ofs_config(config_path->str());
    if (ofs_config.good()) {
      ofs_config << "[DEFAULT]\n";
      ofs_config << "logging_folder =\n";
      ofs_config << "plugin_folder = " << plugin_dir->str() << "\n";
      ofs_config << "runtime_folder = " << stage_dir->str() << "\n";
      ofs_config << "config_folder = " << stage_dir->str() << "\n\n";
      ofs_config << "[logger]" << "\n\n";
      ofs_config.close();
    }
  }

  std::unique_ptr<Path> config_path;
};

TEST_F(Bug22572346, ConfigVarWithIllegalCharAtBeg) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {#mysqld1}\nmode = read-only\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
    "option destinations in [routing:modeReadOnly] has an invalid destination address '{#mysqld1}:3306'");
}

TEST_F(Bug22572346, ConfigVarWithIllegalCharInMid) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {mysqld@1}\nmode = read-only\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
      "option destinations in [routing:modeReadOnly] has an invalid destination address '{mysqld@1}:3306'");
}

TEST_F(Bug22572346, ConfigVarWithIllegalCharAtEnd) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {mysqld1`}\nmode = read-only\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
      "option destinations in [routing:modeReadOnly] has an invalid destination address '{mysqld1`}:3306'");
}

TEST_F(Bug22572346, ConfigVarWithSameMultIllegalChars) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {mysqld!!1}\nmode = read-only\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
      "option destinations in [routing:modeReadOnly] has an invalid destination address '{mysqld!!1}:3306'");
}

TEST_F(Bug22572346, ConfigVarWithDiffMultIllegalChars) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {mysql$d%1}\nmode = read-only\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
      "option destinations in [routing:modeReadOnly] has an invalid destination address '{mysql$d%1}:3306'");
}

TEST_F(Bug22572346, ConfigBindPortWithIllegalChar) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = {mysqld@1}\ndestinations = localhost\nmode = read-only\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
      "option bind_port in [routing:modeReadOnly] needs value between 1 and 65535 inclusive, was '{mysqld@1}'");
}

TEST_F(Bug22572346, ConfigVarWithSpaceAtBeg) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = { mysqld1}\nmode = read-only\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
      "option destinations in [routing:modeReadOnly] has an invalid destination address '{ mysqld1}:3306'");
}

TEST_F(Bug22572346, ConfigVarWithSpaceInMid) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {my sqld1}\nmode = read-only\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
      "option destinations in [routing:modeReadOnly] has an invalid destination address '{my sqld1}:3306'");
}

TEST_F(Bug22572346, ConfigVarWithSpaceAtEnd) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {mysqld1 }\nmode = read-only\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
      "option destinations in [routing:modeReadOnly] has an invalid destination address '{mysqld1 }:3306'");
}

TEST_F(Bug22572346, ConfigVarWithSpaceBeforeIllegalChar) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = { @mysqld1}\nmode = read-only\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
      "option destinations in [routing:modeReadOnly] has an invalid destination address '{ @mysqld1}:3306'");
}

TEST_F(Bug22572346, ConfigVarWithIllegalCharBeforeSpace) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {m@ysql d1}\nmode = read-only\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
      "option destinations in [routing:modeReadOnly] has an invalid destination address '{m@ysql d1}:3306'");
}

TEST_F(Bug22572346, ConfigVarWithMultSpace) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {my sq ld1}\nmode = read-only\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
      "option destinations in [routing:modeReadOnly] has an invalid destination address '{my sq ld1}:3306'");
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

