/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <thread>
#include <unistd.h>

#include "config_parser.h"
#include "filesystem.h"
#include "plugin.h"

#include "cmd_exec.h"
#include "gtest_consoleoutput.h"
#include "helper_logger.h"
#include "mysql_routing.h"
#include "plugin_config.h"

using std::string;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::StrEq;

// define what is available in routing_plugin.cc
extern Plugin harness_plugin_routing;
extern const AppInfo *g_app_info;
extern const char *kRoutingRequires[1];

int init(const AppInfo *info);

void start(const ConfigSection *section);

string g_cwd;
Path g_origin;

class RoutingPluginTests : public ConsoleOutputTest {
protected:
  virtual void SetUp() {
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(g_cwd));
    config_path->append("test_routing_plugin.ini");
    cmd = app_mysqlrouter->str() + " -c " + config_path->str();

    bind_address = "127.0.0.1:15508";
    destinations = "127.0.0.1:3306";
    mode = "read-only";
    connect_timeout = "1";
    client_connect_timeout = "9";
    max_connect_errors = "100";
  }

  bool in_missing(std::vector<std::string> missing, std::string needle) {
    return std::find(missing.begin(), missing.end(), needle) != missing.end();
  }

  void reset_config(std::vector<std::string> missing = {}, bool add_break = false) {
    std::ofstream ofs_config(config_path->str());
    if (ofs_config.good()) {
      ofs_config << "[DEFAULT]\n";
      ofs_config << "logging_folder =\n";
      ofs_config << "plugin_folder = " << plugin_dir->str() << "\n";
      ofs_config << "runtime_folder = " << stage_dir->str() << "\n";
      ofs_config << "config_folder = " << stage_dir->str() << "\n\n";
      ofs_config << "[routing:tests]\n";
      if (!in_missing(missing, "bind_address")) {
        ofs_config << "bind_address = " << bind_address << "\n";
      }
      if (!in_missing(missing, "destinations")) {
        ofs_config << "destinations = " << destinations << "\n";
      }
      if (!in_missing(missing, "mode")) {
        ofs_config << "mode = " << mode << "\n";
      }
      if (!in_missing(missing, "connect_timeout")) {
        ofs_config << "connect_timeout = " << connect_timeout << "\n";
      }
      if (!in_missing(missing, "client_connect_timeout")) {
        ofs_config << "client_connect_timeout = " << client_connect_timeout << "\n";
      }
      if (!in_missing(missing, "max_connect_errors")) {
        ofs_config << "max_connect_errors = " << max_connect_errors << "\n";
      }

      // Following is an incorrect [routing] entry. If the above is valid, this
      // will make sure Router stops.
      if (add_break) {
        ofs_config << "\n[routing:break]\n";
      }

      ofs_config << "\n";
      ofs_config.close();
    }
  }

  virtual void TearDown() {
    if (unlink(config_path->c_str()) == -1) {
      if (errno != ENOENT) {
        // File missing is OK.
        std::cerr << "Failed removing " << config_path->str()
        << ": " << strerror(errno) << "(" << errno << ")" << std::endl;
      }
    }
    ConsoleOutputTest::TearDown();
  }

  const string plugindir = "path/to/plugindir";
  const string logdir = "/path/to/logdir";
  const string program = "routing_plugin_test";
  const string rundir = "/path/to/rundir";
  const string cfgdir = "/path/to/cfgdir";
  string bind_address = "127.0.0.1:15508";
  string destinations = "127.0.0.1:3306";
  string mode = "read-only";
  string connect_timeout = "1";
  string client_connect_timeout = "9";
  string max_connect_errors = "100";

  std::unique_ptr<Path> config_path;
  std::string cmd;
};

TEST_F(RoutingPluginTests, PluginConstants) {
  // Check number of required plugins
  ASSERT_EQ(1UL, sizeof(kRoutingRequires) / sizeof(*kRoutingRequires));
  // Check the required plugins
  ASSERT_THAT(kRoutingRequires[0], StrEq("logger"));
}

TEST_F(RoutingPluginTests, PluginObject) {
  ASSERT_EQ(harness_plugin_routing.abi_version, 0x0100);
  ASSERT_EQ(harness_plugin_routing.plugin_version, VERSION_NUMBER(0, 0, 1));
  ASSERT_EQ(harness_plugin_routing.requires_length, 1);
  ASSERT_THAT(harness_plugin_routing.requires[0], StrEq("logger"));
  ASSERT_EQ(harness_plugin_routing.conflicts_length, 0);
  ASSERT_THAT(harness_plugin_routing.conflicts, IsNull());
  ASSERT_THAT(harness_plugin_routing.deinit, IsNull());
  ASSERT_THAT(harness_plugin_routing.brief,
              StrEq("Routing MySQL connections between MySQL clients/connectors and servers"));
}

TEST_F(RoutingPluginTests, InitAppInfo) {
  ASSERT_THAT(g_app_info, IsNull());

  AppInfo test_app_info{
      program.c_str(),
      plugindir.c_str(),
      logdir.c_str(),
      rundir.c_str(),
      cfgdir.c_str(),
      nullptr
  };

  int res = harness_plugin_routing.init(&test_app_info);
  ASSERT_EQ(res, 0);

  ASSERT_THAT(g_app_info, Not(IsNull()));
  ASSERT_THAT(program.c_str(), StrEq(g_app_info->program));
}

TEST_F(RoutingPluginTests, StartCorrectSection) {
  reset_config({}, true);
  auto cmd_result = cmd_exec(cmd, true);
  ASSERT_THAT(cmd_result.output, HasSubstr("[routing:break]"));
}

TEST_F(RoutingPluginTests, StartMissingMode) {
  reset_config({"mode"});
  auto cmd_result = cmd_exec(cmd, true);
  ASSERT_THAT(cmd_result.output,
              HasSubstr("option mode in [routing:tests] needs to be specified; valid are read-only, read-write"));
}

TEST_F(RoutingPluginTests, StartCaseInsensitiveMode) {
  mode = "Read-Only";
  reset_config({}, true);
  auto cmd_result = cmd_exec(cmd, true);
  ASSERT_THAT(cmd_result.output,
              Not(HasSubstr("valid are read-only, read-write")));
}

TEST_F(RoutingPluginTests, StartMissingDestination) {
  {
    reset_config({"destinations"});
    auto cmd_result = cmd_exec(cmd, true);
    ASSERT_THAT(cmd_result.output,
                HasSubstr("option destinations in [routing:tests] is required"));
  }

  {
    destinations = {};
    reset_config({});
    auto cmd_result = cmd_exec(cmd, true);
    ASSERT_THAT(cmd_result.output,
                HasSubstr("option destinations in [routing:tests] is required and needs a value"));
  }
}

TEST_F(RoutingPluginTests, StartImpossiblePortNumber) {
  bind_address = "127.0.0.1:99999";
  reset_config();
  auto cmd_result = cmd_exec(cmd, true);
  ASSERT_THAT(cmd_result.output,
              HasSubstr("incorrect (invalid TCP port: impossible port number)"));
}

TEST_F(RoutingPluginTests, StartImpossibleIPAddress) {
  bind_address = "512.512.512.512:3306";
  reset_config();
  auto cmd_result = cmd_exec(cmd, true);
  ASSERT_THAT(cmd_result.output,
              HasSubstr("in [routing:tests]: invalid IP or name in bind_address '512.512.512.512:3306'"));
}

TEST_F(RoutingPluginTests, StartWithBindAddressInDestinations) {
  bind_address = "127.0.0.1:3306";
  destinations = "127.0.0.1";  // default port is 3306
  reset_config();
  auto cmd_result = cmd_exec(cmd, true);
  ASSERT_THAT(cmd_result.output, HasSubstr("Bind Address can not be part of destinations"));
}

TEST_F(RoutingPluginTests, StartConnectTimeoutSetNegative) {
  connect_timeout = "-1";
  reset_config();
  auto cmd_result = cmd_exec(cmd, true);
  ASSERT_THAT(cmd_result.output,
              HasSubstr("connect_timeout in [routing:tests] needs value between 1 and 65535 inclusive, was '-1'"));
}

TEST_F(RoutingPluginTests, StartClientConnectTimeoutSetIncorrectly) {
  {
    client_connect_timeout = "1";
    reset_config();
    auto cmd_result = cmd_exec(cmd, true);
    ASSERT_THAT(cmd_result.output, HasSubstr(
        "option client_connect_timeout in [routing:tests] needs value between 2 and 31536000 inclusive, was '1'"));
  }

  {
    client_connect_timeout = "31536001";  // 31536000 is maximum
    reset_config();
    auto cmd_result = cmd_exec(cmd, true);
    ASSERT_THAT(cmd_result.output, HasSubstr(
        "option client_connect_timeout in [routing:tests] needs "
            "value between 2 and 31536000 inclusive, was '31536001'"));
  }
}

TEST_F(RoutingPluginTests, StartMaxConnectErrorsSetIncorrectly) {
  {
    max_connect_errors = "0";
    reset_config();
    auto cmd_result = cmd_exec(cmd, true);
    ASSERT_THAT(cmd_result.output, HasSubstr(
        "option max_connect_errors in [routing:tests] needs value between 1 and 4294967295 inclusive, was '0'"));
  }
}

TEST_F(RoutingPluginTests, StartTimeoutsSetToZero) {

  connect_timeout = "0";
  reset_config();
  auto cmd_result = cmd_exec(cmd, true);
  ASSERT_THAT(cmd_result.output,
              HasSubstr(
                  "option connect_timeout in [routing:tests] needs value between 1 and 65535 inclusive, was '0'"));
}

int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}