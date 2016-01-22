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

enable_testing()

include_directories(${GTEST_INCLUDE_DIRS})

include(HarnessTesting)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/log
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/log/harness
)

add_subdirectory(plugins)

# Some basic tests of the executable. The error message is printed
# to stderr, which will be caught by CTest and checked for matching.
if(ENABLE_HARNESS_PROGRAM)
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
endif()

add_harness_test(TestLoader SOURCES test_loader.cc)

add_harness_test(TestDesignator SOURCES test_designator.cc)
add_harness_test(TestIterator SOURCES test_iterator.cc)
add_harness_test(TestUtilities SOURCES test_utilities.cc)

add_harness_test(TestFilesystem SOURCES test_filesystem.cc)
add_harness_test(TestConfig SOURCES test_config.cc)

add_harness_test(TestBug22104451 SOURCES test_bug22104451.cc)

# Use configuration file templates to generate configuration files
file(GLOB_RECURSE _templates RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cfg.in")
foreach(_template ${_templates})
  string(REGEX REPLACE ".in$" "" _output ${_template})
  message(STATUS "Generating ${_output} from ${_template}")
  configure_file(${_template} ${_output})
endforeach()

# Copy plain configuration files
file(GLOB_RECURSE _files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cfg")
foreach(_file ${_files})
  configure_file(${_file} ${_file} COPYONLY)
endforeach()
