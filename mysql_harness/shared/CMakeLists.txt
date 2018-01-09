# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

add_library(test-helpers STATIC src/test_helpers.cc)
set_target_properties(test-helpers PROPERTIES
  LINKER_LANGUAGE CXX)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # silence undefined use of macro-vars in gtest.
  # we can't use #pragma's due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431 to
  # supress it locally.
  set_target_properties(test-helpers PROPERTIES
    COMPILE_FLAGS "-Wno-undef -Wno-conversion")
endif()
target_include_directories(test-helpers
  PUBLIC include
  PUBLIC ${GMOCK_INCLUDE_DIRS}
  PUBLIC ../harness/include
  PUBLIC ${MySQLRouter_BINARY_DIR}/include)
target_link_libraries(test-helpers
  PUBLIC ${TEST_LIBRARIES} harness-library)
