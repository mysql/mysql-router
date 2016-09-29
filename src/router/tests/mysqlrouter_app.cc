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

#define UNIT_TESTS  // used in router_app.h
#include "config.h"
#include "config_parser.h"
#include "loader.h"
#include "router_app.h"

//ignore GMock warnings
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif

#include "gmock/gmock.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <cstdio>
#include <sstream>
#include <streambuf>
#ifndef _WIN32
#  include <unistd.h>
#endif

#ifdef __clang__
// ignore GMock warnings
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  include "gmock/gmock.h"
#  pragma clang diagnostic pop
#else
#  include "gmock/gmock.h"
#endif

using std::string;
using std::vector;

using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Ge;
using ::testing::NotNull;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::StrEq;

using mysql_harness::Path;

const string get_cwd() {
  char buffer[FILENAME_MAX];
  if (!getcwd(buffer, FILENAME_MAX)) {
    throw std::runtime_error("getcwd failed: " + string(strerror(errno)));
  }
  return string(buffer);
}

Path g_origin;
Path g_stage_dir;

class AppTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    stage_dir = g_stage_dir;
    orig_cout_ = std::cout.rdbuf();
    std::cout.rdbuf(ssout.rdbuf());
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

  Path stage_dir;
  std::stringstream ssout;
  std::streambuf *orig_cout_;
};

TEST_F(AppTest, DefaultConstructor) {
  MySQLRouter r;
  ASSERT_STREQ(MYSQL_ROUTER_VERSION, r.get_version().c_str());
}

TEST_F(AppTest, GetVersionAsString) {
  MySQLRouter r;
  ASSERT_STREQ(MYSQL_ROUTER_VERSION, r.get_version().c_str());
}

TEST_F(AppTest, GetVersionLine) {
  MySQLRouter r;
  ASSERT_THAT(r.get_version_line(), StartsWith(PACKAGE_NAME));
  ASSERT_THAT(r.get_version_line(), HasSubstr(MYSQL_ROUTER_VERSION));
  ASSERT_THAT(r.get_version_line(), HasSubstr(MYSQL_ROUTER_VERSION_EDITION));
  ASSERT_THAT(r.get_version_line(), HasSubstr(PACKAGE_PLATFORM));
  if (PACKAGE_ARCH_64BIT == 1) {
    ASSERT_THAT(r.get_version_line(), HasSubstr("64-bit"));
  } else {
    ASSERT_THAT(r.get_version_line(), HasSubstr("32-bit"));
  }
}

TEST_F(AppTest, CheckConfigFilesSuccess) {
  MySQLRouter r;

  r.default_config_files_ = {};
  r.extra_config_files_ = { stage_dir.join("/etc/mysqlrouter_extra.ini").str() };
  ASSERT_THROW(r.check_config_files(), std::runtime_error);
}

TEST_F(AppTest, CmdLineConfig) {
  vector<string> argv = {
      "--config", stage_dir.join("etc").join("mysqlrouter.ini").str()
  };
  ASSERT_NO_THROW({ MySQLRouter r(g_origin, argv); });
  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(r.get_config_files().at(0), EndsWith("mysqlrouter.ini"));
  ASSERT_THAT(r.get_default_config_files(), IsEmpty());
  ASSERT_THAT(r.get_extra_config_files(), IsEmpty());
}

TEST_F(AppTest, CmdLineConfigFailRead) {
  string not_existing = "foobar.ini";
  vector<string> argv = {
      "--config", stage_dir.join(not_existing).str(),
  };
  ASSERT_THROW({ MySQLRouter r(g_origin, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("Failed reading configuration file"));
    ASSERT_THAT(exc.what(), HasSubstr(not_existing));
  }
}

TEST_F(AppTest, CmdLineMultipleConfig) {
  vector<string> argv = {
      "--config", stage_dir.join("etc").join("mysqlrouter.ini").str(),
      "-c", stage_dir.join("etc").join("config_a.ini").str(),
      "--config", stage_dir.join("etc").join("config_b.ini").str()
  };
  ASSERT_THROW({ MySQLRouter r(g_origin, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
  } catch (const std::runtime_error &exc) {
    ASSERT_THAT(exc.what(), HasSubstr("can only be used once"));
  }
}

TEST_F(AppTest, CmdLineExtraConfig) {
  vector<string> argv = {
      "-c", stage_dir.join("etc").join("config_a.ini").str(),
      "--extra-config", stage_dir.join("etc").join("config_b.ini").str()
  };
  ASSERT_NO_THROW({MySQLRouter r(g_origin, argv);});
  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(r.get_extra_config_files().at(0), EndsWith("config_b.ini"));
  ASSERT_THAT(r.get_default_config_files(), SizeIs(0));
  ASSERT_THAT(r.get_config_files(), SizeIs(1));
}

TEST_F(AppTest, CmdLineExtraConfigFailRead) {
  string not_existing = "foobar.ini";
  vector<string> argv = {
      "-c", stage_dir.join("etc").join("config_a.ini").str(),
      "--extra-config", stage_dir.join("etc").join(not_existing).str()
  };
  ASSERT_THROW({ MySQLRouter r(g_origin, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("Failed reading configuration file"));
    ASSERT_THAT(exc.what(), EndsWith(not_existing));
  }
}

TEST_F(AppTest, CmdLineMultipleExtraConfig) {
  vector<string> argv = {
      "-c", stage_dir.join("etc").join("mysqlrouter.ini").str(),
      "-a", stage_dir.join("etc").join("config_a.ini").str(),
      "--extra-config", stage_dir.join("etc").join("config_b.ini").str()
  };
  ASSERT_NO_THROW({MySQLRouter r(g_origin, argv);});
  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(r.get_config_files().at(0).c_str(), EndsWith("mysqlrouter.ini"));
  ASSERT_THAT(r.get_extra_config_files().at(0).c_str(), EndsWith("config_a.ini"));
  ASSERT_THAT(r.get_extra_config_files().at(1).c_str(), EndsWith("config_b.ini"));
  ASSERT_THAT(r.get_default_config_files(), SizeIs(0));
  ASSERT_THAT(r.get_config_files(), SizeIs(1));
}

TEST_F(AppTest, CmdLineMultipleDuplicateExtraConfig) {
  string duplicate = "config_a.ini";
  vector<string> argv = {
      "-c", stage_dir.join("etc").join("config_a.ini").str(),
      "--extra-config", stage_dir.join("etc").join("mysqlrouter.ini").str(),
      "-a", stage_dir.join("etc").join(duplicate).str(),
      "--extra-config", stage_dir.join("etc").join(duplicate).str(),
  };
  ASSERT_THROW({ MySQLRouter r(g_origin, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("Duplicate configuration file"));
    ASSERT_THAT(exc.what(), HasSubstr(duplicate));
  }
}

TEST_F(AppTest, CmdLineExtraConfigNoDeafultFail) {
  string duplicate = "config_a.ini";
  vector<string> argv = {
      "--extra-config", stage_dir.join("etc").join("mysqlrouter.ini").str(),
  };
  ASSERT_THROW({ MySQLRouter r(g_origin, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("Extra configuration files only work when other "));
  }
}

TEST_F(AppTest, CmdLineVersion) {
  vector<string> argv = {"--version"};

  reset_ssout();

  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(ssout.str(), StartsWith(r.get_version_line()));
}

TEST_F(AppTest, CmdLineVersionShort) {
  vector<string> argv = {"-v"};

  reset_ssout();

  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(ssout.str(), StartsWith("MySQL Router"));
}

TEST_F(AppTest, ConfigFileParseError) {
  vector<string> argv = {
      "--config", stage_dir.join("etc").join("parse_error.ini").str(),
  };
  ASSERT_THROW({ MySQLRouter r(g_origin, argv); r.start(); }, std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
    r.start();
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("Configuration error: Malformed section header:"));
  }
}

TEST_F(AppTest, SectionOverMultipleConfigFiles) {
  string extra_config = stage_dir.join("etc").join("mysqlrouter_extra.ini").str();
  vector<string> argv = {
      "--config", stage_dir.join("etc").join("mysqlrouter.ini").str(),
      "--extra-config=" + extra_config
  };
  ASSERT_NO_THROW({MySQLRouter r(g_origin, argv);});

  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(r.get_config_files().at(0).c_str(), EndsWith("mysqlrouter.ini"));
  ASSERT_THAT(r.get_extra_config_files().at(0).c_str(), EndsWith("mysqlrouter_extra.ini"));

  r.start();
  ASSERT_NO_THROW(r.start());
  /* Functionality missing in Harness Loader to get section
  auto section = r.loader_->config_.get("logger", "");
  ASSERT_THAT(section.get("foo"), StrEq("bar"));
  ASSERT_THROW(section.get("NotInTheSection"), bad_option);
  */
}

#ifndef _WIN32
TEST_F(AppTest, CanStartTrue) {
  vector<string> argv = {
      "--config", stage_dir.join("etc").join("mysqlrouter.ini").str()
  };
  ASSERT_NO_THROW({MySQLRouter r(g_origin, argv);});
}

TEST_F(AppTest, CanStartFalse) {
  vector<vector<string> > cases = {
      {""},
  };
  for(auto &argv: cases) {
    ASSERT_THROW({MySQLRouter r(g_origin, argv); r.start();}, std::runtime_error);
  }
}

TEST_F(AppTest, ShowingInfoTrue) {

  vector<vector<string> > cases = {
      {"--version"},
      {"--help"},
      {"--help", "--config", stage_dir.join("etc").join("mysqlrouter.ini").str()},
      {"--config", stage_dir.join("etc").join("mysqlrouter.ini").str(), "--help"},
  };

  // Make sure we do not start when showing information
  for(auto &argv: cases) {
    ASSERT_NO_THROW({MySQLRouter r(g_origin, argv); r.start();});
    ASSERT_THAT(ssout.str(), HasSubstr("MySQL Router v"));
    reset_ssout();
  }
}

TEST_F(AppTest, ShowingInfoFalse) {
  // Cases should be allowing Router to start
  vector<vector<string> > cases = {
      {"--config", stage_dir.join("etc").join("mysqlrouter.ini").str(),},
  };

  for(auto &argv: cases) {
    ASSERT_NO_THROW({MySQLRouter r(g_origin, argv); r.start();});
  }
}
#endif

int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();

  if (char *stage_dir_env = std::getenv("STAGE_DIR")) {
    g_stage_dir = Path(stage_dir_env).real_path();
  } else {
    // try a few places
    g_stage_dir = Path(get_cwd()).join("..").join("..").join("stage");
    if (!g_stage_dir.is_directory()) {
      g_stage_dir = Path(get_cwd()).join("stage");
    }
  }
#ifdef _WIN32
  g_stage_dir = g_stage_dir.join(g_origin.basename());
#endif

  if (!g_stage_dir.is_directory()) {
    std::cout << "Stage dir not valid (was " << g_stage_dir << "; can use STAGE_DIR env var)" << std::endl;
    return -1;
  }

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
