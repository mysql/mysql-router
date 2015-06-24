#include <mysql/harness/filesystem.h>

#include <iostream>
#include <vector>

#include "helpers.h"

using std::cout;
using std::endl;
using std::back_inserter;

void
test_path(const std::string& program)
{
  Path here = Path(program).dirname();

  // Testing basic path construction
  expect_equal(Path("/data/logger.cfg"), "/data/logger.cfg");
  expect_equal(Path("data/logger.cfg"), "data/logger.cfg");
  expect_equal(Path("/"), "/");
  expect_equal(Path("//"), "/");
  expect_equal(Path("////////"), "/");
  expect_equal(Path("/data/"), "/data");
  expect_equal(Path("data/"), "data");
  expect_equal(Path("data////"), "data");

  // Testing error usage
  expect_exception<std::runtime_error>([]{ Path path(""); });

  // Testing dirname function
  expect_equal(Path("foo.cfg").dirname(), ".");
  expect_equal(Path("foo/bar.cfg").dirname(), "foo");
  expect_equal(Path("/foo/bar.cfg").dirname(), "/foo");
  expect_equal(Path("/").dirname(), "/");

  // Testing basename function
  expect_equal(Path("foo.cfg").basename(), "foo.cfg");
  expect_equal(Path("foo/bar.cfg").basename(), "bar.cfg");
  expect_equal(Path("/foo/bar.cfg").basename(), "bar.cfg");
  expect_equal(Path("/").basename(), "/");

  // Testing join function (and indirectly the append function).
  Path new_path = Path("data").join("test");
  expect_equal(new_path, "data/test");

  // Testing file status checking functions
  expect_equal(here.join("data").type(),
               Path::FileType::DIRECTORY_FILE);
  expect_equal(here.join("data/logger.cfg").type(),
               Path::FileType::REGULAR_FILE);
  expect_equal(here.join("data/does-not-exist.cfg").type(),
               Path::FileType::FILE_NOT_FOUND);
  expect_equal(here.join("data").is_directory(), true);
  expect_equal(here.join("data/logger.cfg").is_directory(), false);
  expect_equal(here.join("data").is_regular(), false);
  expect_equal(here.join("data/logger.cfg").is_regular(), true);
}

void
test_directory(const std::string& program)
{
  Path dirname = Path(program).dirname();
  Directory directory(dirname.join("data"));

  {
    // These are the files in the "data" directory in the test
    // directory. Please update it if you add more files.
    std::vector<Path> expect{
      dirname.join("data/logger.d"),
      dirname.join("data/logger.cfg"),
      dirname.join("data/tests-bad-1.cfg"),
      dirname.join("data/tests-bad-2.cfg"),
      dirname.join("data/tests-bad-3.cfg"),
      dirname.join("data/tests-good-1.cfg"),
      dirname.join("data/magic-alt.cfg"),
    };

    decltype(expect) result;
    std::copy(directory.begin(), directory.end(), back_inserter(result));
    std::sort(expect.begin(), expect.end());
    std::sort(result.begin(), result.end());
    expect_equal(result, expect);
  }

  {
    // These are files in the "data" directory in the test
    // directory. Please update it if you add more files.
    std::vector<Path> expect{
      dirname.join("data/tests-bad-1.cfg"),
      dirname.join("data/tests-bad-2.cfg"),
      dirname.join("data/tests-bad-3.cfg"),
    };
    decltype(expect) result;
    std::copy(directory.glob("tests-bad*.cfg"), directory.end(),
              back_inserter(result));
    std::sort(expect.begin(), expect.end());
    std::sort(result.begin(), result.end());
    expect_equal(result, expect);
  }
}

int main(int argc, char *argv[])
{
  try {
    test_path(argv[0]);
    test_directory(argv[0]);
  }
  catch (std::runtime_error& exc) {
    std::cerr << exc.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
