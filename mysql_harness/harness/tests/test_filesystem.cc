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

Path g_here;

TEST(TestFilesystem, TestPath)
{
  // Testing basic path construction
  EXPECT_EQ(Path("/data/logger.cfg"), "/data/logger.cfg");
  EXPECT_EQ(Path("data/logger.cfg"), "data/logger.cfg");
  EXPECT_EQ(Path("/"), "/");
  EXPECT_EQ(Path("//"), "/");
  EXPECT_EQ(Path("////////"), "/");
  EXPECT_EQ(Path("/data/"), "/data");
  EXPECT_EQ(Path("data/"), "data");
  EXPECT_EQ(Path("data////"), "data");

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
  EXPECT_EQ(g_here.join("data").type(),
            Path::FileType::DIRECTORY_FILE);

  EXPECT_EQ(g_here.join("data/logger.cfg").type(),
               Path::FileType::REGULAR_FILE);
  EXPECT_EQ(g_here.join("data/does-not-exist.cfg").type(),
               Path::FileType::FILE_NOT_FOUND);

  EXPECT_TRUE(g_here.join("data").is_directory());
  EXPECT_FALSE(g_here.join("data/logger.cfg").is_directory());
  EXPECT_FALSE(g_here.join("data").is_regular());
  EXPECT_TRUE(g_here.join("data/logger.cfg").is_regular());
}

TEST(TestFilesystem, EmptyPath) {
  // Testing error usage
  EXPECT_THROW({ Path path(""); }, std::invalid_argument);

  // Default-constructed paths should be possible to create, but not
  // to use.
  Path path;
  EXPECT_THROW(path.is_regular(), std::invalid_argument);
  EXPECT_THROW(path.is_directory(), std::invalid_argument);
  EXPECT_THROW(path.type(), std::invalid_argument);
  EXPECT_THROW(path.append(g_here), std::invalid_argument);
  EXPECT_THROW(path.join(g_here), std::invalid_argument);
  EXPECT_THROW(path.basename(), std::invalid_argument);
  EXPECT_THROW(path.dirname(), std::invalid_argument);
  EXPECT_THROW(g_here.append(path), std::invalid_argument);
  EXPECT_THROW(g_here.join(path), std::invalid_argument);

  // Once a real path is moved into it, all should be fine.
  path = g_here;
  EXPECT_EQ(path, g_here);
  EXPECT_TRUE(path.is_directory());
  EXPECT_FALSE(path.is_regular());
}


TEST(TestFilesystem, TestDirectory)
{
  Directory directory(g_here.join("data"));

  {
    // These are the files in the "data" directory in the test
    // directory. Please update it if you add more files.
    //
    // TODO: Do not use the data directory for this but create a
    // dedicated directory for testing this feature.
    std::vector<Path> expect{
      g_here.join("data/logger.d"),
      g_here.join("data/logger.cfg"),
      g_here.join("data/tests-bad-1.cfg"),
      g_here.join("data/tests-bad-2.cfg"),
      g_here.join("data/tests-bad-3.cfg"),
      g_here.join("data/tests-good-1.cfg"),
      g_here.join("data/tests-good-2.cfg"),
      g_here.join("data/magic-alt.cfg"),
    };

    decltype(expect) result(directory.begin(), directory.end());
    EXPECT_SETEQ(expect, result);
  }

  {
    // These are files in the "data" directory in the test
    // directory. Please update it if you add more files.
    std::vector<Path> expect{
      g_here.join("data/tests-bad-1.cfg"),
      g_here.join("data/tests-bad-2.cfg"),
      g_here.join("data/tests-bad-3.cfg"),
    };

    decltype(expect) result(directory.glob("tests-bad*.cfg"), directory.end());
    EXPECT_SETEQ(expect, result);
  }
}

int main(int argc, char *argv[])
{
  g_here = Path(argv[0]).dirname();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
