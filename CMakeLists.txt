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

project(harness)

cmake_minimum_required (VERSION 2.8.10)

if(${CMAKE_VERSION} VERSION_GREATER "3.0")
  cmake_policy(SET CMP0042 NEW) # MACOSX_RPATH is enabled by default
  cmake_policy(SET CMP0048 NEW)
  cmake_policy(SET CMP0046 NEW)
endif()

if(${CMAKE_VERSION} VERSION_GREATER "3.1")
  cmake_policy(SET CMP0054 NEW)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Basic variables
set(HARNESS_NAME "harness" CACHE STRING
  "Name of Harness")
set(INSTALL_INCLUDE_DIR "include/mysql/${HARNESS_NAME}" CACHE STRING
  "Installation directory for harness include files")
set(HARNESS_INSTALL_PROGRAM YES CACHE BOOL
  "Whether to install the Harness main program")
set(HARNESS_INSTALL_PLUGINS YES CACHE BOOL
  "Whether to install the Harness provided plugins")
set(ENABLE_TESTS OFF CACHE BOOL
  "Enable unit tests when building")

# We include GMock without touching the compile flags. GMock can
# handle that itself. It will also indirectly create targets for gmock
# and gtest.
#
# Two alternatives for locating GMock *source code*:
# 1. If WITH_GMOCK is given, this is expected to be the location of
#    the *source code*.
# 2. If WITH_GMOCK is not given, it will look in the 'ext' directory
#    in the source root.
if(ENABLE_TESTS)
  message(STATUS "Looking for GMock under ${CMAKE_SOURCE_DIR}/ext/gmock/CMakeLists.txt")
  if(WITH_GMOCK)
    set(_root ${WITH_GMOCK})
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/ext/gmock/CMakeLists.txt")
    set(_root "${CMAKE_SOURCE_DIR}/ext/gmock")
  else()
    message(SEND_ERROR
      "Unable to find GMock, not possible to build tests. Either "
      "disable tests with ENABLE_TESTS=no or download the source code "
      "for GMock (available at ).")
  endif()
  add_subdirectory(${_root} ext/gmock)

  set(TEST_LIBRARIES gmock gtest gmock_main gtest_main)

  # Since GMock and GTest do not set
  # INTERFACE_SYSTEM_INCLUDE_DIRECTORIES, we do that here. This means
  # that any targets that reference one of these libraries will
  # "automatically" have the include directories for these libraries
  # added to their build flags.
  target_include_directories(gmock SYSTEM PUBLIC ext/gmock/include)
  target_include_directories(gmock_main SYSTEM PUBLIC ext/gmock/include)
  target_include_directories(gtest SYSTEM PUBLIC ext/gmock/gtest/include)
  target_include_directories(gtest_main SYSTEM PUBLIC ext/gmock/gtest/include)
endif()

include(CPack)
include(Plugin)
include(Docs)

# Check for C++ 11 support
include(CheckCXXCompilerFlag)
if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  # This declares that we are POSIX.1-2001 compliant and
  # XSI-conforming. The pre-processor variable _POSIX_C_SOURCE is
  # indirectly set when we set _XOPEN_SOURCE.
  add_definitions(-D_XOPEN_SOURCE=600)

  check_cxx_compiler_flag("-std=c++11" COMPILER_SUPPORTS_CXX11)
  check_cxx_compiler_flag("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
  if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
  else()
    message(STATUS "You need C++11 support, but ${CMAKE_CXX_COMPILER} does not have that.")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -Wconversion -Wpedantic")
  if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_COMPILER_IS_GNUCXX VERSION_LESS "4.9")
  endif()
else()
  message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} not supported")
endif()

if(COMPILER_SUPPORTS_CXX11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
  message(FATAL_ERROR "You need C++11 support, but ${CMAKE_CXX_COMPILER} does not have that.")
endif()

# Check for basic functions
find_package(Threads REQUIRED)

add_subdirectory(harness)
add_subdirectory(plugins)

if(ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
