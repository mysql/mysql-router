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

#include "config.h"
#define orig_private private
#define private public
#include "router_app.h"
#define private orig_private

#include "gmock/gmock.h"

#include <cstdio>
#include <sstream>
#include <streambuf>
#include <unistd.h>

#include "loader.h"
#include "config_parser.h"

using std::string;
using std::vector;

using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Ge;
using ::testing::NotNull;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::StrEq;

const string get_cwd() {
  char buffer[FILENAME_MAX];
  getcwd(buffer, FILENAME_MAX);
  return string(buffer);
}

Path g_origin;

class AppTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    char *stage_dir_c = std::getenv("STAGE_DIR");
    {
      SCOPED_TRACE("Make sure environment variable STAGE_DIR is set");
      ASSERT_THAT(stage_dir_c, NotNull());
    }
    stage_dir = string(stage_dir_c);
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

  string stage_dir;
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
  r.extra_config_files_ = { stage_dir + "/etc/mysqlrouter_extra.ini" };
  ASSERT_THROW(r.check_config_files(), std::runtime_error);
}

TEST_F(AppTest, CmdLineConfig) {
  vector<string> argv = {
      "--config", stage_dir + "/etc/mysqlrouter.ini"
  };
  ASSERT_NO_THROW({ MySQLRouter r(g_origin, argv); });
  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(r.config_files_.at(0).c_str(), HasSubstr(argv.at(1).c_str()));
  ASSERT_THAT(r.default_config_files_, IsEmpty());
  ASSERT_THAT(r.extra_config_files_, IsEmpty());
}

TEST_F(AppTest, CmdLineConfigFailRead) {
  string not_existing = "foobar.ini";
  vector<string> argv = {
      "--config", stage_dir + not_existing,
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
      "--config", stage_dir + "/etc/mysqlrouter.ini",
      "-c", stage_dir + "/etc/config_a.ini",
      "--config", stage_dir + "/etc/config_b.ini"
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
      "-c", stage_dir + "/etc/config_a.ini",
      "--extra-config", stage_dir + "/etc/config_b.ini"
  };
  ASSERT_NO_THROW({MySQLRouter r(g_origin, argv);});
  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(r.extra_config_files_.at(0).c_str(), HasSubstr(argv.at(3).c_str()));
  ASSERT_THAT(r.default_config_files_, SizeIs(0));
  ASSERT_THAT(r.config_files_, SizeIs(1));
}

TEST_F(AppTest, CmdLineExtraConfigFailRead) {
  string not_existing = "foobar.ini";
  vector<string> argv = {
      "-c", stage_dir + "/etc/config_a.ini",
      "--extra-config", stage_dir + not_existing,
  };
  ASSERT_THROW({ MySQLRouter r(g_origin, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("Failed reading configuration file"));
    ASSERT_THAT(exc.what(), HasSubstr(not_existing));
  }
}

TEST_F(AppTest, CmdLineMultipleExtraConfig) {
  vector<string> argv = {
      "-c", stage_dir + "/etc/mysqlrouter.ini",
      "-a", stage_dir + "/etc/config_a.ini",
      "--extra-config", stage_dir + "/etc/config_b.ini"
  };
  ASSERT_NO_THROW({MySQLRouter r(g_origin, argv);});
  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(r.config_files_.at(0).c_str(), HasSubstr(argv.at(1).c_str()));
  ASSERT_THAT(r.extra_config_files_.at(0).c_str(), HasSubstr(argv.at(3).c_str()));
  ASSERT_THAT(r.extra_config_files_.at(1).c_str(), HasSubstr(argv.at(5).c_str()));
  ASSERT_THAT(r.default_config_files_, SizeIs(0));
  ASSERT_THAT(r.config_files_, SizeIs(1));
}

TEST_F(AppTest, CmdLineMultipleDuplicateExtraConfig) {
  string duplicate = "config_a.ini";
  vector<string> argv = {
      "-c", stage_dir + "/etc/config_a.ini",
      "--extra-config", stage_dir + "/etc/mysqlrouter.ini",
      "-a", stage_dir + "/etc/" + duplicate,
      "--extra-config", stage_dir + "/etc/" + duplicate
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
      "--extra-config", stage_dir + "/etc/mysqlrouter.ini",
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
      "--config", stage_dir + "/etc/parse_error.ini",
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
  string extra_config = stage_dir + "/etc/mysqlrouter_extra.ini";
  vector<string> argv = {
      "--config", stage_dir + "/etc/mysqlrouter.ini",
      "--extra-config=" + extra_config
  };
  ASSERT_NO_THROW({MySQLRouter r(g_origin, argv);});

  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(r.config_files_.at(0).c_str(), HasSubstr(argv.at(1).c_str()));
  ASSERT_THAT(r.extra_config_files_.at(0).c_str(), HasSubstr(extra_config.c_str()));

  r.start();
  auto section = r.loader_->config_.get("logger", "");
  ASSERT_THAT(section.get("foo"), StrEq("bar"));
  ASSERT_THROW(section.get("NotInTheSection"), bad_option);
}

TEST_F(AppTest, CanStartTrue) {
  vector<string> argv = {
      "--config", stage_dir + "/etc/mysqlrouter.ini",
  };
  ASSERT_NO_THROW({MySQLRouter r(g_origin, argv);});
  MySQLRouter r(g_origin, argv);
  ASSERT_TRUE(r.can_start_);
}

TEST_F(AppTest, CanStartFalse) {
  MySQLRouter r;

  vector<vector<string> > cases = {
      {},
      {"--version"},
      {"--help"}
  };

  for(auto &argv: cases) {
    try {
      r.init(argv);
    } catch (const std::runtime_error &exc) {
      ASSERT_FALSE(r.can_start_);
    }
  }
}

TEST_F(AppTest, ShowingInfoTrue) {
  MySQLRouter r;

  vector<vector<string> > cases = {
      {"--version"},
      {"--help"},
      {"--help", "--config", stage_dir + "/etc/mysqlrouter.ini"},
      {"--config", stage_dir + "/etc/mysqlrouter.ini", "--help"},
  };

  for(auto &argv: cases) {
    try {
      r.init(argv);
    } catch (const std::runtime_error &exc) {
      ASSERT_TRUE(r.showing_info_);
      ASSERT_FALSE(r.can_start_);
    }
  }
}

TEST_F(AppTest, ShowingInfoFalse) {
  MySQLRouter r;

  vector<vector<string> > cases = {
      {},
      {"--config", stage_dir + "/etc/mysqlrouter.ini"},
  };

  for(auto &argv: cases) {
    try {
      r.init(argv);
    } catch (const std::runtime_error &exc) {
      ASSERT_FALSE(r.showing_info_);
    }
  }
}


int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

