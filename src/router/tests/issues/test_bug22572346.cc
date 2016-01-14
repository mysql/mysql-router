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

/**
 * BUG22572346 CORE DUMP WHILE STARTING THE ROUTER WHEN DESTINATIONS HAS @ CHARACTER
 *
 */
#include "gtest_consoleoutput.h"
#include "router_app.h"
#include "config_parser.h"

#include <fstream>
#include <string>

#include "gmock/gmock.h"

using std::string;
using ::testing::StrEq;

string g_cwd;
Path g_origin;

class Bug22572346 : public ConsoleOutputTest {
protected:
  virtual void SetUp() {
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(g_cwd));
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

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (std::runtime_error &err) {
    ASSERT_THAT(err.what(), StrEq(
      "Only alphanumeric characters in variable names allowed. Saw '#'"));
  }
}

TEST_F(Bug22572346, ConfigVarWithIllegalCharInMid) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {mysqld@1}\nmode = read-only\n";
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (std::runtime_error &err) {
    ASSERT_THAT(err.what(), StrEq(
      "Only alphanumeric characters in variable names allowed. Saw 'mysqld@'"));
  }
}

TEST_F(Bug22572346, ConfigVarWithIllegalCharAtEnd) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {mysqld1`}\nmode = read-only\n";
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (std::runtime_error &err) {
    ASSERT_THAT(err.what(), StrEq(
      "Only alphanumeric characters in variable names allowed. Saw 'mysqld1`'"));
  }
}

TEST_F(Bug22572346, ConfigVarWithSameMultIllegalChars) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {mysqld!!1}\nmode = read-only\n";
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (std::runtime_error &err) {
    ASSERT_THAT(err.what(), StrEq(
      "Only alphanumeric characters in variable names allowed. Saw 'mysqld!'"));
  }
}

TEST_F(Bug22572346, ConfigVarWithDiffMultIllegalChars) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {mysql$d%1}\nmode = read-only\n";
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (std::runtime_error &err) {
    ASSERT_THAT(err.what(), StrEq(
      "Only alphanumeric characters in variable names allowed. Saw 'mysql$'"));
  }
}

TEST_F(Bug22572346, ConfigBindPortWithIllegalChar) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = {mysqld@1}\ndestinations = localhost\nmode = read-only\n";
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (std::runtime_error &err) {
    ASSERT_THAT(err.what(), StrEq(
      "Only alphanumeric characters in variable names allowed. Saw 'mysqld@'"));
  }
}

TEST_F(Bug22572346, ConfigVarWithSpaceAtBeg) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = { mysqld1}\nmode = read-only\n";
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (std::runtime_error &err) {
    ASSERT_THAT(err.what(), StrEq(
      "Only alphanumeric characters in variable names allowed. Saw ' '"));
  }
}

TEST_F(Bug22572346, ConfigVarWithSpaceInMid) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {my sqld1}\nmode = read-only\n";
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (std::runtime_error &err) {
    ASSERT_THAT(err.what(), StrEq(
      "Only alphanumeric characters in variable names allowed. Saw 'my '"));
  }
}

TEST_F(Bug22572346, ConfigVarWithSpaceAtEnd) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {mysqld1 }\nmode = read-only\n";
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (std::runtime_error &err) {
    ASSERT_THAT(err.what(), StrEq(
      "Only alphanumeric characters in variable names allowed. Saw 'mysqld1 '"));
  }
}

TEST_F(Bug22572346, ConfigVarWithSpaceBeforeIllegalChar) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = { @mysqld1}\nmode = read-only\n";
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (std::runtime_error &err) {
    ASSERT_THAT(err.what(), StrEq(
      "Only alphanumeric characters in variable names allowed. Saw ' '"));
  }
}

TEST_F(Bug22572346, ConfigVarWithIllegalCharBeforeSpace) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {m@ysql d1}\nmode = read-only\n";
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (std::runtime_error &err) {
    ASSERT_THAT(err.what(), StrEq(
      "Only alphanumeric characters in variable names allowed. Saw 'm@'"));
  }
}

TEST_F(Bug22572346, ConfigVarWithMultSpace) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:modeReadOnly]\nbind_port = 7001\ndestinations = {my sq ld1}\nmode = read-only\n";
  c.close();

  auto r = MySQLRouter(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (std::runtime_error &err) {
    ASSERT_THAT(err.what(), StrEq(
      "Only alphanumeric characters in variable names allowed. Saw 'my '"));
  }
}

int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

