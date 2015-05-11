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

cmake_minimum_required(VERSION 2.8)

include(cmake/settings.cmake)
include(cmake/version.cmake)

if(${CMAKE_VERSION} VERSION_GREATER "3.0")
    cmake_policy(SET CMP0042 NEW)
    cmake_policy(SET CMP0048 NEW)
    project(${MYSQL_PROJECT_NAME} VERSION ${PROJECT_VERSION_TEXT} LANGUAGES C CXX)
else()
    project(${MYSQL_PROJECT_NAME})
endif()

# Required tools and libraries
find_package(Threads REQUIRED)
include(cmake/boost.cmake)
include(cmake/python.cmake)

# Enable testing
if(ENABLE_TESTS)
    include(cmake/gtest.cmake)
    include(cmake/testing.cmake)
    enable_testing()
    add_subdirectory(tests)
endif()

# Compiler & Linker settings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Load all modules, including plugins
add_subdirectory(src)
