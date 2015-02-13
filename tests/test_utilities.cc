#include "utilities.h"

#include <stdexcept>
#include <string>
#include <cstring>
#include <iostream>

static void check_dirname(const std::string& path,
                          const std::string& expected)
{
  std::string result = dirname(path);
  if (result != expected)
  {
    char buf[256];
    sprintf(buf, "dirname('%s') was '%s', expected '%s'",
            path.c_str(), result.c_str(), expected.c_str());
    throw std::runtime_error(std::string(buf));
  }
}

static void check_basename(const std::string& path,
                           const std::string& expected)
{
  std::string result = basename(path);
  if (result != expected)
  {
    char buf[256];
    sprintf(buf, "basename('%s') was '%s', expected '%s'",
            path.c_str(), result.c_str(), expected.c_str());
    throw std::runtime_error(std::string(buf));
  }
}

static int test_dirname() 
{
  check_dirname("foo", ".");
  check_dirname("foo/bar", "foo");
  check_dirname("foo/bar/baz", "foo/bar");
}

static int test_basename()
{
  check_basename("foo", "foo");
  check_basename("foo/bar", "bar");
  check_basename("foo/bar/baz", "baz");
}

int main()
{
  try {
    test_dirname();
    test_basename();
  }
  catch (std::runtime_error& exc) {
    std::cerr << exc.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}

