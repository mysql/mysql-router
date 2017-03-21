/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _WIN32 // this test fails on Windows due to Git/shell problems

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

#ifdef GTEST_USES_POSIX_RE
#include <regex.h>
#endif
using mysql_harness::Path;

struct GitInfo {
  Path file;
  int year_first_commit;
  int year_last_commit;
};

Path g_origin;
Path g_source_dir;
std::vector<GitInfo> g_git_tracked_files;
bool g_skip_git_tests = false;

const std::vector<std::string> kLicenseSnippets{
    "This program is free software; you can redistribute it",
    "under the terms of the GNU General Public License",
    "version 2",
    "",
    "This program is distributed in the hope that",
    "02110-1301", // last line of the copyright header
};

// Ignored file extensions
const std::vector<std::string> kIgnoredExtensions{
    ".o", ".pyc", ".pyo", ".conf.in", ".cfg.in", ".cfg", ".html", ".css", ".conf", ".ini",
};

const std::vector<std::string> kIgnoredFileNames{
    ".gitignore",
    "nt_servc.cc",
    "nt_servc.h",
    "License.txt",
    "Doxyfile.in",
#ifndef _WIN32
    "README.md" // symlink on Unix-like, doesn't work on Windows
#endif
};

// Paths to ignore; relative to repository root
const std::vector<Path> kIgnoredPaths{
    Path("mysql_harness"),  // we can not check fully subtrees
    Path("packaging"),
    Path("internal"),
    Path(".git"),
    Path(".idea"),
    Path("build"),
    Path("ext"),
};

bool is_ignored_path(Path path, const std::vector<Path> ignored_paths) {
  // Check paths we are ignoring
  Path fullpath(Path(g_source_dir).real_path());
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
    Path real_path = tmp_path.real_path();
    if (!real_path.is_set()) {
      std::cerr << "realpath failed for " << tracked_file << ": " << strerror(errno) << std::endl;
      continue;
    }
    tracked_file = real_path.str();
    if (!is_ignored(tracked_file)) {
      os_cmd.str("");
      os_cmd << "git log HEAD --pretty=format:%ad --date=short --diff-filter=AM -- " << tracked_file;
      result = cmd_exec(os_cmd.str(), false, g_source_dir.str());
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
      if (!g_skip_git_tests) {
        prepare_git_tracked_files();
      }
    }

    virtual void TearDown() {
    }
};

/* test if the all files that are in git have the proper copyright line
 *
 * A proper copyright line is:
 *
 * - copyright years: if start year == end year, start year may be omitted
 * - copyright start year: at least first git commit
 * - copyright end year: at least last git commit
 * - copyright line: fixed format
 *
 * The copyright years start before recorded history in git as the files
 * may come from another source. Similar to end date as git author-date
 * may contain too old date.
 */
TEST_F(CheckLegal, Copyright) {
  SKIP_GIT_TESTS(g_skip_git_tests)
  ASSERT_THAT(g_git_tracked_files.size(), ::testing::Gt(static_cast<size_t>(0)));

#ifdef GTEST_USES_POSIX_RE
  // gtest uses either simple-re or posix-re. Only the posix-re supports captures
  // which allows to extract the dates easily.
  //
  // if gtest uses posix-re, we can use posix-re directly too.
  const char *prefix = "Copyright (c)";
  std::string needle;
  needle = "Copyright \\(c\\) (([0-9]{4}), )?";  // m[1] and m[2]
  needle += "([0-9]{4}), ";                      // m[3]
  needle += "Oracle and/or its affiliates. All rights reserved.";

  // extract the years
  regex_t re;
  char re_err[1024];
  regmatch_t m[4];
  int err_code = regcomp(&re, needle.c_str(), REG_EXTENDED);
  if (err_code != 0) {
    EXPECT_LE(regerror(err_code, &re, re_err, sizeof(re_err)), sizeof(re_err));
    ASSERT_EQ(err_code, 0) << re_err;
  }

  for (auto &it: g_git_tracked_files) {
    std::ifstream curr_file(it.file.str());

    std::string line;
    bool copyright_found = false;

    while (std::getline(curr_file, line, '\n')) {
      if (line.find(prefix) != std::string::npos) {

        EXPECT_THAT(line, ::testing::ContainsRegex(needle)) << " in file: " << it.file.str();

        // match the needly again, but this time extract the copyright years.
        err_code = regexec(&re, line.c_str(), sizeof(m) / sizeof(regmatch_t), m, 0);
        if (err_code != 0) {
          EXPECT_LE(regerror(err_code, &re, re_err, sizeof(re_err)), sizeof(re_err));
          EXPECT_EQ(err_code, 0) << re_err;
          continue;
        }

        // check that the start copyright year is less or equal to what we have a commit for
        //
        // allow copyright years that are less than the recorded history in git
        ASSERT_GT(m[3].rm_so, 0) << m[3].rm_so;
        std::string copyright_end_year = line.substr(m[3].rm_so, m[3].rm_eo - m[3].rm_so);

        if (m[2].rm_so != -1) {
          std::string copyright_start_year = line.substr(m[2].rm_so, m[2].rm_eo - m[2].rm_so);
          EXPECT_LE(std::stoi(copyright_start_year), it.year_first_commit) << " in file: " << it.file.str();
        } else {
          // no start-year in copyright.
          EXPECT_LE(std::stoi(copyright_end_year), it.year_first_commit) << " in file: " << it.file.str();
        }

        // copyright end year has to at least the one of the last commit
        //
        // allow copyright years that are larger than the recorded history in git
        EXPECT_GE(std::stoi(copyright_end_year), it.year_last_commit) << " in file: " << it.file.str();

        copyright_found = true;
        break;
      }
    }
    curr_file.close();

    EXPECT_TRUE(copyright_found) << it.file.str() << ": No copyright found";
  }

  regfree(&re);
#endif
}

TEST_F(CheckLegal, GPLLicense) {
  SKIP_GIT_TESTS(g_skip_git_tests)
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
      problem = "";
      if (line.find(kLicenseSnippets[index]) != std::string::npos) {
        found = true;
        index++;
        if (index == kLicenseSnippets.size()) {// matched last line
          break;
        }
        continue;
      }
    }
    curr_file.close();

    if (!found) {
      problem = "No license";
    } else if (index != kLicenseSnippets.size()) {
      problem = "Content of license not correct";
    }
    EXPECT_TRUE(problem.empty()) << "Problem in " << it.file << ": " << problem;
  }
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  g_origin = Path(argv[0]).dirname();
  g_source_dir = get_cmake_source_dir();

  EXPECT_TRUE(g_source_dir.is_set());
  EXPECT_TRUE(Path(g_source_dir).join(".git").is_directory());

  return RUN_ALL_TESTS();
}

#else

int main(int, char*) {
  return 0;
}

#endif // #ifndef _WIN32
