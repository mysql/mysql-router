# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

set(WITH_HARNESS "${PROJECT_SOURCE_DIR}/mysql_harness" CACHE PATH "Folder containing MySQL Harness sources")

if(NOT (EXISTS ${WITH_HARNESS}/CMakeLists.txt AND EXISTS ${WITH_HARNESS}/harness/src/loader.cc))
  message(FATAL_ERROR "MySQL Harness source folder not available. Please use cmake -DWITH_HARNESS to point "
    "to MySQL Harness sources. We tried '${WITH_HARNESS}'.")
endif()

message(STATUS "Adding MySQL Harness from ${WITH_HARNESS}")

set(ENABLE_HARNESS_PROGRAM NO CACHE BOOL "Harness program is not installed")
if (WIN32)
  set(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${MySQLRouter_BINARY_STAGE_DIR}/lib/ CACHE STRING "Output directory for plugins")
else()
  set(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${MySQLRouter_BINARY_STAGE_DIR}/lib/${HARNESS_NAME} CACHE STRING "Output directory for plugins")
endif()
set(HARNESS_INSTALL_LIBRARY_DIR "${INSTALL_LIBDIR}" CACHE PATH "Installation directory for Harness libraries")

# binary_dir needed when WITH_HARNESS is out-of-tree
add_subdirectory(${WITH_HARNESS} ${PROJECT_BINARY_DIR}/harness)

include_directories(${WITH_HARNESS}/harness/include)
