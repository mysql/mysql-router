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

////////////////////////////////////////
// Harness interface include files
#include "loader.h"
#include "plugin.h"
#include "filesystem.h"

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

////////////////////////////////////////
// Third-party include files
#include "gtest/gtest.h"

////////////////////////////////////////
// Standard include files
#include <iostream>
#include <fstream>

using std::cout;
using std::endl;

std::string g_here;

class KeepalivePluginTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    Path here = Path(g_here);
    orig_cout_ = std::cout.rdbuf();
    std::cout.rdbuf(ssout.rdbuf());

    std::map<std::string, std::string> params;
    params["program"] = "harness";
    params["prefix"] = here.c_str();

    loader = new Loader("harness", params);
    loader->read(here.join("data/keepalive.cfg"));
  }

  virtual void TearDown() {
    std::cout.rdbuf(orig_cout_);
    delete loader;
    loader = nullptr;
  }

  Loader *loader;

private:
  std::stringstream ssout;
  std::streambuf *orig_cout_;
};

TEST_F(KeepalivePluginTest, Available) {
  auto lst = loader->available();
  EXPECT_EQ(2U, lst.size());

  EXPECT_SECTION_AVAILABLE("keepalive", loader);
  EXPECT_SECTION_AVAILABLE("logger", loader);
}

TEST_F(KeepalivePluginTest, CheckLog) {
  const auto log_file = loader->get_log_file();

  // Make sure log file is empty
  std::fstream fs;
  fs.open(log_file.str(), std::fstream::trunc | std::ofstream::out);
  fs.close();

  loader->start();

  std::ifstream ifs_log(log_file.str());
  std::string line;
  std::vector<std::string> lines;
  while (std::getline(ifs_log, line)) {
    lines.push_back(line);
  }
  EXPECT_NE(std::string::npos, lines.at(0).find("keepalive started with interval 1") );
  EXPECT_NE(std::string::npos, lines.at(1).find("2 time(s)") );
  EXPECT_NE(std::string::npos, lines.at(2).find("keepalive") );
  EXPECT_NE(std::string::npos, lines.at(3).find("INFO") );
  EXPECT_NE(std::string::npos, lines.at(3).find("keepalive") );
}

int main(int argc, char *argv[])
{
  g_here = Path(argv[0]).dirname().str();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
