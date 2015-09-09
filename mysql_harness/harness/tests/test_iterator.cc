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

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

////////////////////////////////////////
// Third-party include files
#include "gtest/gtest.h"

////////////////////////////////////////
// Standard include files
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

TEST(TestIterator, TestIterator) {
  static const char* array[] = {
    "one", "two", "three",
  };
  const size_t array_length = sizeof(array)/sizeof(*array);
  const char** ptr = array;

  auto range = make_range(array, array_length);
  for (auto elem: range) {
    EXPECT_EQ(elem, *ptr);
    EXPECT_LT(ptr - array,
              static_cast<long>(sizeof(array)/sizeof(*array)));
    ++ptr;
  }
}
