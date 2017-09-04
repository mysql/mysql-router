# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
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
