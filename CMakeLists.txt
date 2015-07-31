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

cmake_minimum_required (VERSION 2.8.10)

if(${CMAKE_VERSION} VERSION_GREATER "3.0")
    cmake_policy(SET CMP0042 NEW) # MACOSX_RPATH is enabled by default
    cmake_policy(SET CMP0048 NEW)
endif()

project(harness)

set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(${CMAKE_VERSION} VERSION_GREATER "3.0")
  cmake_policy(SET CMP0046 NEW)
  cmake_policy(SET CMP0054 NEW)
endif()

# Basic variables
set(HARNESS_NAME "harness" CACHE STRING "Name of Harness")
set(INSTALL_INCLUDE_DIR "include/mysql/harness")
set(HARNESS_INSTALL_PROGRAM YES CACHE BOOL "Whether to install the Harness main program")
set(HARNESS_INSTALL_PLUGINS YES CACHE BOOL "Whether to install the Harness provided plugins")

include(CPack)
include(Plugin)
include(Docs)

# Check for C++ 11 support
include(CheckCXXCompilerFlag)
if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
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

enable_testing()
add_subdirectory(harness)
add_subdirectory(tests)
add_subdirectory(plugins)

