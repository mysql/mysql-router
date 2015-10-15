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

#include "gmock/gmock.h"

#include <chrono>
#include <fstream>
#include <thread>

#include "config_parser.h"
#include "filesystem.h"
#include "plugin.h"

#include "mysql_routing.h"
#include "plugin_config.h"
#include "helper_logger.h"

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

// some globals
using LogLines = std::vector<std::string>;
string cwd;

class RoutingPluginTests : public ::testing::Test {
public:
  AppInfo test_app_info;
  std::shared_ptr<ConfigSection> config_section;
protected:
  virtual void SetUp() {
    orig_cout_ = std::cout.rdbuf(ssout.rdbuf());

    test_app_info = {
        program.c_str(),
        plugindir.c_str(),
        logdir.c_str(),
        rundir.c_str(),
        cfgdir.c_str(),
        nullptr
    };

    config_section = std::make_shared<ConfigSection>("routing", "tests", nullptr);
    config_section->add("bind_address", "127.0.0.1:15508");
    config_section->add("destinations", "127.0.0.1:3306");
    config_section->add("mode", "read-only");
  }

  virtual void TearDown() {
    if (orig_cout_) {
      std::cout.rdbuf(orig_cout_);
    }
  }

  void reset_ssout() {
    ssout.str("");
    ssout.clear();
  }


  const string plugindir = "path/to/plugindir";
  const string logdir = "/path/to/logdir";
  const string program = "routing_plugin_test";
  const string rundir = "/path/to/rundir";
  const string cfgdir = "/path/to/cfgdir";

  std::stringstream ssout;

private:
  FILE *log_fp_;
  string log_file_;
  std::streambuf *orig_cout_;
};

TEST_F(RoutingPluginTests, PluginConstants) {
  ASSERT_EQ(sizeof(kRoutingRequires)/sizeof(*kRoutingRequires), 1);
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

  int res = harness_plugin_routing.init(&test_app_info);
  ASSERT_EQ(res, 0);

  ASSERT_THAT(g_app_info, Not(IsNull()));
  ASSERT_THAT(program.c_str(), StrEq(g_app_info->program));
}

TEST_F(RoutingPluginTests, StartCorrectSection) {
  harness_plugin_routing.init(&test_app_info);
  auto start = harness_plugin_routing.start;

  std::thread thr(start, config_section.get());
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // The event loop will start; we can't cleanly exit at this time
  thr.detach();

  auto log = ssout.str();
  ASSERT_THAT(log, HasSubstr("routing:tests started: listening on 127.0.0.1:15508"));
}

TEST_F(RoutingPluginTests, StartMissingMode) {
  auto section = config_section.get();
  section->set("mode", "");

  harness_plugin_routing.init(&test_app_info);
  auto start = harness_plugin_routing.start;

  start(section);
  auto log = ssout.str();
  ASSERT_THAT(log, HasSubstr("option mode in [routing:tests] needs to be specified; valid are read-only, read-write"));
}

TEST_F(RoutingPluginTests, StartCamelCaseMode) {
  auto section = config_section.get();
  section->set("mode", "ReaD-WRiTe");

  harness_plugin_routing.init(&test_app_info);

  auto start = harness_plugin_routing.start;

  start(section);
  auto log = ssout.str();
  ASSERT_THAT(log,
      Not(HasSubstr("Section [routing:tests]: Mode needs to be specified; valid are read-only, read-write")));
}

TEST_F(RoutingPluginTests, StartMissingDestination) {
  auto section = config_section.get();
  section->set("destinations", "");

  harness_plugin_routing.init(&test_app_info);
  auto start = harness_plugin_routing.start;

  start(section);
  auto log = ssout.str();
  ASSERT_THAT(log, HasSubstr("option destinations in [routing:tests] is required and needs a value"));
}

TEST_F(RoutingPluginTests, StartImpossiblePortNumber) {
  auto section = config_section.get();
  section->set("bind_address", "127.0.0.1:99999");  // impossible IP address

  harness_plugin_routing.init(&test_app_info);
  auto start = harness_plugin_routing.start;

  start(section);
  auto log = ssout.str();
  ASSERT_THAT(log, HasSubstr(
      "option bind_address in [routing:tests] is incorrect (invalid TCP port: impossible port number)"));
}

TEST_F(RoutingPluginTests, StartImpossibleIPAddress) {
  auto section = config_section.get();
  section->set("bind_address", "512.512.512.512:3306");  // impossible IP address

  harness_plugin_routing.init(&test_app_info);
  auto start = harness_plugin_routing.start;

  start(section);
  auto log = ssout.str();
  ASSERT_THAT(log, HasSubstr(
      "routing:tests: Setting up service using 512.512.512.512:3306: Failed getting address information"));
}

TEST_F(RoutingPluginTests, StartWithBindAddressInDestinations) {
  auto section = config_section.get();
  section->set("bind_address", "127.0.0.1:3306");
  section->set("destinations", "127.0.0.1");   // default port is 3306

  harness_plugin_routing.init(&test_app_info);
  auto start = harness_plugin_routing.start;

  start(section);
  auto log = ssout.str();
  ASSERT_THAT(log, HasSubstr(
      "Bind Address can not be part of destinations"));
}

TEST_F(RoutingPluginTests, StartConnectTimeoutSetNegative) {
  auto section = config_section.get();
  section->set("connect_timeout", "-1");

  harness_plugin_routing.init(&test_app_info);
  auto start = harness_plugin_routing.start;

  start(section);
  auto log = ssout.str();
  ASSERT_THAT(log, HasSubstr(
      "connect_timeout in [routing:tests] needs value between 1 and 65535 inclusive, was '-1'"));
}


TEST_F(RoutingPluginTests, StartTimeoutsSetToZero) {
  auto section = config_section.get();
  section->set("bind_address", "127.0.0.1:15509");
  section->set("connect_timeout", "0");

  harness_plugin_routing.init(&test_app_info);
  auto start = harness_plugin_routing.start;

  start(section);
  auto log = ssout.str();
  ASSERT_THAT(log, HasSubstr(
      "option connect_timeout in [routing:tests] needs value between 1 and 65535 inclusive, was '0'"));
}
