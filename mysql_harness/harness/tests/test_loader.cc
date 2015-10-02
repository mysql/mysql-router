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

#include "loader.h"

#include "exception.h"
#include "filesystem.h"
#include "plugin.h"
#include "utilities.h"

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

////////////////////////////////////////
// Third-party include files
#include "gtest/gtest.h"

////////////////////////////////////////
// Standard include files
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::endl;

Path g_here;

class LoaderTest
  : public ::testing::TestWithParam<const char*>
{
protected:
  virtual void SetUp() {
    std::map<std::string, std::string> params;
    params["program"] = "harness";
    params["prefix"] = g_here.c_str();

    loader = new Loader("harness", params);
  }

  virtual void TearDown() {
    delete loader;
    loader = nullptr;
  }

  Loader *loader;
};

class LoaderReadTest
  : public LoaderTest
{
protected:
  virtual void SetUp() {
    LoaderTest::SetUp();
    loader->read(Path(g_here).join(GetParam()));
  }
};

TEST_P(LoaderReadTest, Available) {
  auto lst = loader->available();
  EXPECT_EQ(6U, lst.size());

  EXPECT_SECTION_AVAILABLE("example", loader);
  EXPECT_SECTION_AVAILABLE("magic", loader);
}

TEST_P(LoaderReadTest, Loading) {
  // These should fail, for different reasons

  // Test that loading something non-existant works
  EXPECT_THROW(loader->load("nonexistant-plugin"), bad_section);

  // Dependent plugin do not exist
  EXPECT_THROW(loader->load("bad_one"), bad_section);

  // Wrong version of dependent sections
  EXPECT_THROW(loader->load("bad_two"), bad_plugin);

  // These should all be OK.
  Plugin* ext1 = loader->load("example", "one");
  EXPECT_NE(ext1, nullptr);
  EXPECT_STREQ("An example plugin", ext1->brief);

  Plugin* ext2 = loader->load("example", "two");
  EXPECT_NE(ext2, nullptr);
  EXPECT_STREQ("An example plugin", ext2->brief);

  Plugin* ext3 = loader->load("magic");
  EXPECT_NE(ext3, nullptr);
  EXPECT_STREQ("A magic plugin", ext3->brief);
}

const char *good_cfgs[] = {
  "data/tests-good-1.cfg",
  "data/tests-good-2.cfg",
};

INSTANTIATE_TEST_CASE_P(TestLoaderGood, LoaderReadTest, ::testing::ValuesIn(good_cfgs));

TEST_P(LoaderTest, BadSection) {
  EXPECT_THROW(loader->read(g_here.join(GetParam())), bad_section);
}

const char *bad_cfgs[] = {
  "data/tests-bad-1.cfg",
  "data/tests-bad-2.cfg",
  "data/tests-bad-3.cfg",
};

INSTANTIATE_TEST_CASE_P(TestLoaderBad, LoaderTest, ::testing::ValuesIn(bad_cfgs));

int main(int argc, char *argv[])
{
  g_here = Path(argv[0]).dirname();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
