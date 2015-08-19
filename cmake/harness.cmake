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

set(WITH_HARNESS "${PROJECT_SOURCE_DIR}/harness" CACHE PATH "Folder containing MySQL Harness sources")

if(NOT (EXISTS ${WITH_HARNESS}/CMakeLists.txt AND EXISTS ${WITH_HARNESS}/harness/loader.cc))
  message(FATAL_ERROR "MySQL Harness source folder not available. Please use cmake -DWITH_HARNESS to point "
    "to MySQL Harness sources. We tried '${WITH_HARNESS}'.")
endif()

message(STATUS "Adding MySQL Harness")

set(HARNESS_NAME "mysqlrouter" CACHE STRING "Name of Harness")
set(HARNESS_INSTALL_PROGRAM NO CACHE BOOL "Harness program is not installed")

add_subdirectory(${WITH_HARNESS} ${CMAKE_BINARY_DIR}/harness)

# Quick fixing tests
file(COPY ${WITH_HARNESS}/tests/data DESTINATION ${CMAKE_BINARY_DIR}/harness/tests)

# Make sure targets are going to staging
set_target_properties(harness-library harness-archive PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${STAGE_DIR}/lib)
set_target_properties(logger PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${STAGE_DIR}/lib/${HARNESS_NAME})

include_directories(${WITH_HARNESS}/harness)
include_directories(${CMAKE_BINARY_DIR}/include)
