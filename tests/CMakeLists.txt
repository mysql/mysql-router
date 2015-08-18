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

include_directories(${PROJECT_SOURCE_DIR}/harness)
include_directories(${CMAKE_BINARY_DIR}/include)

enable_testing()

find_package(GMock)
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_subdirectory(plugins)

macro(MY_ADD_TEST name)
  set(doing)
  set(sources)
  set(requires)
  foreach(arg ${ARGN})
    if(arg MATCHES "^(SOURCES|REQUIRES)$")
      set(doing ${arg})
    elseif(doing MATCHES "^SOURCES$")
      list(APPEND sources ${arg})
    elseif(doing MATCHES "^REQUIRES")
      list(APPEND requires "${arg}-INTERFACE")
    else()
      message(AUTHOR_WARNING "Unknown argument: '${arg}'")
    endif()
  endforeach()

  add_executable(${name} ${sources})
  if(requires)
    add_dependencies(${name} ${requires})
  endif()
  target_link_libraries(${name} harness-archive ${GTEST_BOTH_LIBRARIES})
  add_test(${name} ${name})
endmacro()

my_add_test(TestLoader SOURCES test_loader.cc REQUIRES harness)
add_custom_command(TARGET TestLoader POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:logger> ${CMAKE_CURRENT_BINARY_DIR}/var/lib/harness/$<TARGET_FILE_NAME:logger>)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/lib
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/log
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/lib/harness
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/log/harness
)

# Some basic tests of the executable. The error message is printed
# to stderr, which will be caught by CTest and checked for matching.
add_test(NAME TestHarness1
  COMMAND $<TARGET_FILE:harness-program> --foobar)
set_tests_properties(TestHarness1 PROPERTIES
  PASS_REGULAR_EXPRESSION "unrecognized option")

add_test(NAME TestHarness2
  COMMAND $<TARGET_FILE:harness-program>)
set_tests_properties(TestHarness2 PROPERTIES
  PASS_REGULAR_EXPRESSION "No configuration file provided")

add_test(NAME TestHarness3
  COMMAND $<TARGET_FILE:harness-program> whatever.cfg)
set_tests_properties(TestHarness3 PROPERTIES
  PASS_REGULAR_EXPRESSION "Path.*whatever.cfg.*does not exist")

my_add_test(TestDesignator SOURCES test_designator.cc)
my_add_test(TestIterator SOURCES test_iterator.cc)
my_add_test(TestUtilities SOURCES test_utilities.cc)

my_add_test(TestFilesystem SOURCES test_filesystem.cc REQUIRES harness)
my_add_test(TestConfig SOURCES test_config.cc REQUIRES harness)

add_custom_command(TARGET TestConfig POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/tests/data ${CMAKE_BINARY_DIR}/tests/data)
