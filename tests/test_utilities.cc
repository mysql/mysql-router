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

#include "utilities.h"

#include "helpers.h"

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

static void test_dirname()
{
  check_dirname("foo", ".");
  check_dirname("foo/bar", "foo");
  check_dirname("foo/bar/baz", "foo/bar");
}

static void test_basename()
{
  check_basename("foo", "foo");
  check_basename("foo/bar", "bar");
  check_basename("foo/bar/baz", "baz");
}

static void test_strip()
{
  const char *strings[][2] = {
    { "foo", "foo", },
    { " foo", "foo", },
    { "foo ", "foo", },
    { " \tfoo \t\t", "foo", },
    { "", "" },
  };

  for (auto sample: make_range(strings, sizeof(strings)/sizeof(*strings)))
  {
    std::string str(sample[0]);
    strip(str);
    expect_equal(str, sample[1]);
  }
}


int main()
{
  try {
    test_dirname();
    test_basename();
    test_strip();
  }
  catch (std::runtime_error& exc) {
    std::cerr << exc.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
