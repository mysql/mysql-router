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
#include "filesystem.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "gmock/gmock.h"

struct GitInfo {
  Path file;
  int year_first_commit;
  int year_last_commit;
};

Path g_origin;
Path g_source_dir;
std::vector<GitInfo> g_git_tracked_files;

const std::vector<std::string> kShortLicense{
    "This program is free software; you can redistribute it and/or modify",
    "it under the terms of the GNU General Public License as published by",
    "the Free Software Foundation; version 2 of the License.",
    "",
    "This program is distributed in the hope that it will be useful,",
    "but WITHOUT ANY WARRANTY; without even the implied warranty of",
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the",
    "GNU General Public License for more details.",
    "",
    "You should have received a copy of the GNU General Public License",
    "along with this program; if not, write to the Free Software",
    "Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA",
};

// Ignored file extensions
const std::vector<std::string> kIgnoredExtensions{
    ".o", ".pyc", ".pyo", ".ini.in", ".cfg.in", ".cfg", ".html", ".css", ".ini",
};

const std::vector<std::string> kIgnoredFileNames{
    ".gitignore",
    "License.txt",
    "Doxyfile.in",
};

// Paths to ignore; relative to repository root
const std::vector<Path> kIgnoredPaths{
    Path("mysql_harness"),  // we can not check fully subtrees
    Path("packaging"),
    Path("internal"),
    Path(".git"),
    Path(".idea"),
    Path("build"),
};

bool is_ignored_path(Path path, const std::vector<Path> ignored_paths) {
  // Check paths we are ignoring
  Path fullpath(realpath(g_source_dir.c_str(), nullptr));
  for (auto &it: ignored_paths) {
    auto tmp = Path(fullpath).join(it);
    if (tmp == path) {
      return true;
    }
    auto tmp_dirname = path.dirname().str();
    if (path.dirname().str().find(tmp.str()) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool is_ignored(const std::string &filepath) {
  Path p(filepath);
  std::string dirname = p.dirname().str();
  std::string basename = p.basename().str();

  // Check extensions
  for (auto &it: kIgnoredExtensions) {
    if (ends_with(basename, it)) {
      return true;
    }
  }

  return std::find(kIgnoredFileNames.begin(), kIgnoredFileNames.end(), basename) != kIgnoredFileNames.end() ||
         is_ignored_path(p, kIgnoredPaths);
}

void prepare_git_tracked_files() {
  if (!g_git_tracked_files.empty()) {
    return;
  }
  // Get all files in the Git repository
  std::ostringstream os_cmd;
  // For Git v1.7 we need to change directory first
  os_cmd << "git ls-files --error-unmatch";
  auto result = cmd_exec(os_cmd.str(), false, g_source_dir.str());
  std::istringstream cmd_output(result.output);
  std::string tracked_file;

  while (std::getline(cmd_output, tracked_file, '\n')) {
    Path tmp_path(g_source_dir);
    tmp_path.append(tracked_file);
    char *real_path = realpath(tmp_path.c_str(), nullptr);
    if (!real_path) {
      std::cerr << "realpath failed for " << tracked_file << ": " << strerror(errno) << std::endl;
      continue;
    }
    tracked_file = std::string(realpath(tmp_path.c_str(), nullptr));
    if (!is_ignored(tracked_file)) {
      os_cmd.str("");
      os_cmd << "git log master --pretty=format:%ad --date=short --diff-filter=AM -- " << tracked_file;
      result = cmd_exec(os_cmd.str());
      // Result should contain at least 1 line with a year.
      if (result.output.size() < 10) {
        std::cerr << "Failed getting Git log info for " << tracked_file << std::endl;
        continue;
      }
      try {
        g_git_tracked_files.push_back(GitInfo{
            Path(tracked_file),
            // Both first and year last modification could be the same
            std::stoi(result.output.substr(result.output.size() - 10, 4)),
            std::stoi(result.output.substr(0, 4))
        });
      } catch (...) {
        std::cerr << "Failed conversion: " << result.output << " , " << tracked_file << std::endl;
      }
    }
  }
}

class CheckLegal : public ::testing::Test {
  protected:
    virtual void SetUp() {
      prepare_git_tracked_files();
    }

    virtual void TearDown() {
    }
};

TEST_F(CheckLegal, Copyright) {
  ASSERT_THAT(g_git_tracked_files.size(), ::testing::Gt(static_cast<size_t>(0)));

  std::vector<std::string> problems;

  for (auto &it: g_git_tracked_files) {
    std::ifstream curr_file(it.file.str());

    std::string line;
    std::string needle;
    std::string problem;
    bool found = false;

    while (std::getline(curr_file, line, '\n')) {
      problem = "";
      if (line.find("Copyright (c)") != std::string::npos
          && ends_with(line, "Oracle and/or its affiliates. All rights reserved.")) {
        found = true;
        // Check first year of first commit is in the copyright
        needle = "(c) " + std::to_string(it.year_first_commit) + ",";
        if (line.find(needle) == std::string::npos) {
          problem = std::string("First commit year ") + std::to_string(it.year_first_commit)
                    + std::string(" not present");
        } else if (it.year_first_commit != it.year_last_commit) {
          // Then check modification year
          needle = std::to_string(it.year_last_commit) + ",";
          if (line.find(needle) == std::string::npos) {
            problem = std::string("Last modification year ") + std::to_string(it.year_last_commit) +
                      std::string(" not present");
          }
        }
        break;
      }
    }
    curr_file.close();

    if (!found) {
      problem = "No copyright statement";
    }

    if (!problem.empty()) {
      std::string tmp = it.file.str();
      tmp.erase(0, g_source_dir.str().size() + 1);
      problems.push_back(tmp + ": " + problem);
    }
  }

  if (!problems.empty()) {
    std::string tmp{"\nCopyright issues in " + g_source_dir.str() + ":\n"};
    std::ostringstream ostr;
    std::copy(problems.begin(), problems.end(), std::ostream_iterator<std::string>(ostr, "\n"));
    EXPECT_TRUE(problems.empty()) << tmp << ostr.str();
  }
}

TEST_F(CheckLegal, GPLLicense) {
  ASSERT_THAT(g_git_tracked_files.size(), ::testing::Gt(static_cast<size_t>(0)));

  std::vector<Path> extra_ignored{
      Path("README.txt"),
  };

  for (auto &it: g_git_tracked_files) {

    if (is_ignored_path(Path(it.file), extra_ignored)) {
      continue;
    }

    std::string line;
    std::string problem;
    bool found = false;
    size_t index = 0;

    std::ifstream curr_file(it.file.str());
    while (std::getline(curr_file, line, '\n')) {
      if (index == kShortLicense.size()) {
        break;
      }
      problem = "";
      if (line.find(kShortLicense[0]) != std::string::npos) {
        found = true;
        index = 1;
        continue;
      }

      if (found) {
        // Check empty line
        if (kShortLicense[index].empty() && (line.empty() || line == "#")) {
          ++index;
          continue;
        } else if (!kShortLicense[index].empty() && line.find(kShortLicense[index]) != std::string::npos) {
          ++index;
          continue;
        } else {
          problem = "Content of license not correct (line: " + std::to_string(index + 1) + ")";
          break;
        }
      }
    }
    curr_file.close();

    if (!found) {
      problem = "No license";
    }

    EXPECT_TRUE(problem.empty()) << "Problem in " << it.file << ": " << problem;
  }
}

int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();
  g_source_dir = get_cmake_source_dir();

  if (!Path(g_source_dir).join(".git").is_directory()) {
    std::cerr << "Test can only run in Git repository" << std::endl;
    return 1;
  }

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

