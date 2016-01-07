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

#include "cmd_exec.h"
#include "router_test_helpers.h"

#include <sstream>
#include <streambuf>
#include <unistd.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

Path g_origin;
Path g_stage_dir;
Path g_mysqlrouter_exec;
bool g_in_git_repo = false;

const int kFirstYear = 2015;

std::string g_help_output_raw;
std::vector<std::string> g_help_output;

class ConsoleOutputTest : public ::testing::Test {
  protected:
    virtual void SetUp() {
      if (g_help_output.empty()) {
        std::ostringstream cmd;
        cmd << g_mysqlrouter_exec << " --help";
        auto result = cmd_exec(cmd.str());
        std::string line;
        std::istringstream iss(result.output);
        while (std::getline(iss, line)) {
          g_help_output.push_back(line);
        }
        g_help_output_raw = result.output;
      }
    }

    virtual void TearDown() {
    }
};

TEST_F(ConsoleOutputTest, Copyright) {
  int last_year = 0;

  if (g_in_git_repo) {
    // We need year of last commit. This year has to be present in copyright.
    std::ostringstream os_cmd;
    os_cmd << "git log --pretty=format:%ad --date=short -1";
    auto result = cmd_exec(os_cmd.str());
    last_year = std::stoi(result.output.substr(0, 4));
    std::cout << last_year << std::endl;
  }

  for (auto &line: g_help_output) {
    if (starts_with(line, "Copyright")) {
      ASSERT_THAT(line, ::testing::HasSubstr(std::to_string(kFirstYear) + ",")) << "Start year not in copyright";
      // following is checked only when in Git repository
      if (last_year > kFirstYear) {
        ASSERT_THAT(line, ::testing::HasSubstr(std::to_string(last_year) + ",")) << "Last year not in copyright";
      }
      break;
    }
  }
}

TEST_F(ConsoleOutputTest, Trademark) {

  for (auto &line: g_help_output) {
    if (starts_with(line, "Oracle is a registered trademark of Oracle")) {
      break;
    }
  }
}

TEST_F(ConsoleOutputTest, ConfigurationFileList) {
  bool found = false;
  std::vector<std::string> config_files;
  std::string indent = "  ";

  for (auto it = g_help_output.begin(); it < g_help_output.end(); ++it) {
    auto line = *it;
    if (found) {
      if (line.empty()) {
        break;
      }
      if (starts_with(line, indent)) {
        auto file = line.substr(indent.size(), line.size());
        config_files.push_back(file);
      }
    }
    if (starts_with(line, "Configuration read")) {
      it++; // skip next line
      found = true;
    }
  }

  ASSERT_TRUE(found) << "Failed reading location configuration locations";
  ASSERT_TRUE(config_files.size() >= 2) << "Failed getting at least 2 configuration file locations";
}

TEST_F(ConsoleOutputTest, BasicUsage) {
  std::vector<std::string> options{
      "[-v|--version]",
      "[-h|--help]",
      "[-c|--config=<path>]",
      "[-a|--extra-config=<path>]",
  };

  for (auto option: options) {
    ASSERT_THAT(g_help_output_raw, ::testing::HasSubstr(option));
  }
}

TEST_F(ConsoleOutputTest, BasicOptionDescriptions) {
  std::vector<std::string> options{
      "  -v, --version",
      "        Display version information and exit.",
      "  -h, --help",
      "        Display this help and exit.",
      "  -c <path>, --config <path>",
      "        Only read configuration from given file.",
      "  -a <path>, --extra-config <path>",
      "        Read this file after configuration files are read",
  };

  for (auto option: options) {
    ASSERT_THAT(g_help_output_raw, ::testing::HasSubstr(option));
  }
}


int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();
  auto binary_dir = get_envvar_path("CMAKE_BINARY_DIR", Path(get_cwd()));
  auto source_dir = get_envvar_path("CMAKE_SOURCE_DIR", Path(".."));

  g_stage_dir = binary_dir.join("stage");
  g_mysqlrouter_exec = g_stage_dir.join("bin").join("mysqlrouter");
  if (!g_mysqlrouter_exec.is_regular()) {
    throw std::runtime_error(
        "mysqlrouter not available. Use CMAKE_BINARY_DIR environment "
            "variable to point to out-of-course build directory.");
  }

  if (!Path(source_dir).join(".git").is_directory()) {
    g_in_git_repo = true;
    return 1;
  }

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}