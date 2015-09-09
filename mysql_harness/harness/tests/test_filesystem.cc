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
#include <vector>

using std::cout;
using std::endl;
using std::back_inserter;

std::string g_here;

TEST(TestFilesystem, TestPath)
{
  Path here = Path(g_here);

  // Testing basic path construction
  EXPECT_EQ(Path("/data/logger.cfg"), "/data/logger.cfg");
  EXPECT_EQ(Path("data/logger.cfg"), "data/logger.cfg");
  EXPECT_EQ(Path("/"), "/");
  EXPECT_EQ(Path("//"), "/");
  EXPECT_EQ(Path("////////"), "/");
  EXPECT_EQ(Path("/data/"), "/data");
  EXPECT_EQ(Path("data/"), "data");
  EXPECT_EQ(Path("data////"), "data");

  // Testing error usage
  EXPECT_THROW({ Path path(""); }, std::runtime_error);

  // Testing dirname function
  EXPECT_EQ(Path("foo.cfg").dirname(), ".");
  EXPECT_EQ(Path("foo/bar.cfg").dirname(), "foo");
  EXPECT_EQ(Path("/foo/bar.cfg").dirname(), "/foo");
  EXPECT_EQ(Path("/").dirname(), "/");

  // Testing basename function
  EXPECT_EQ(Path("foo.cfg").basename(), "foo.cfg");
  EXPECT_EQ(Path("foo/bar.cfg").basename(), "bar.cfg");
  EXPECT_EQ(Path("/foo/bar.cfg").basename(), "bar.cfg");
  EXPECT_EQ(Path("/").basename(), "/");

  // Testing join function (and indirectly the append function).
  Path new_path = Path("data").join("test");
  EXPECT_EQ(new_path, "data/test");

  // Testing file status checking functions
  EXPECT_EQ(here.join("data").type(),
            Path::FileType::DIRECTORY_FILE);

  EXPECT_EQ(here.join("data/logger.cfg").type(),
               Path::FileType::REGULAR_FILE);
  EXPECT_EQ(here.join("data/does-not-exist.cfg").type(),
               Path::FileType::FILE_NOT_FOUND);

  EXPECT_TRUE(here.join("data").is_directory());
  EXPECT_FALSE(here.join("data/logger.cfg").is_directory());
  EXPECT_FALSE(here.join("data").is_regular());
  EXPECT_TRUE(here.join("data/logger.cfg").is_regular());
}

TEST(TestFilesystem, TestDirectory)
{
  Path here(g_here);
  Directory directory(here.join("data"));

  {
    // These are the files in the "data" directory in the test
    // directory. Please update it if you add more files.
    //
    // TODO: Do not use the data directory for this but create a
    // dedicated directory for testing this feature.
    std::vector<Path> expect{
      here.join("data/logger.d"),
      here.join("data/logger.cfg"),
      here.join("data/tests-bad-1.cfg"),
      here.join("data/tests-bad-2.cfg"),
      here.join("data/tests-bad-3.cfg"),
      here.join("data/tests-good-1.cfg"),
      here.join("data/tests-good-2.cfg"),
      here.join("data/magic-alt.cfg"),
    };

    decltype(expect) result(directory.begin(), directory.end());
    EXPECT_SETEQ(expect, result);
  }

  {
    // These are files in the "data" directory in the test
    // directory. Please update it if you add more files.
    std::vector<Path> expect{
      here.join("data/tests-bad-1.cfg"),
      here.join("data/tests-bad-2.cfg"),
      here.join("data/tests-bad-3.cfg"),
    };

    decltype(expect) result(directory.glob("tests-bad*.cfg"), directory.end());
    EXPECT_SETEQ(expect, result);
  }
}

int main(int argc, char *argv[])
{
  g_here = Path(argv[0]).dirname().str();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
