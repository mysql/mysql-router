# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

cmake_minimum_required (VERSION 2.8.12)

if(${CMAKE_VERSION} VERSION_GREATER "3.0")
  cmake_policy(SET CMP0042 NEW) # MACOSX_RPATH is enabled by default
  cmake_policy(SET CMP0048 NEW)
  cmake_policy(SET CMP0046 NEW)
endif()

if(${CMAKE_VERSION} VERSION_GREATER "3.1")
  cmake_policy(SET CMP0054 NEW)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")

# Basic variables
set(HARNESS_NAME "harness" CACHE STRING
  "Name of Harness")

if("${STAGE_DIR}" STREQUAL "")
    set(STAGE_DIR ${CMAKE_BINARY_DIR}/stage CACHE INTERNAL "STAGE_DIR")
endif()
set(HARNESS_INSTALL_LIBRARY_DIR "lib" CACHE PATH
    "Installation directory for Harness libraries")
set(HARNESS_INSTALL_BIN_DIR "bin" CACHE PATH
    "Installation directory for Harness main binaries")    
set(ENABLE_HARNESS_PROGRAM YES CACHE BOOL
  "Whether to build and install the Harness main program")
set(HARNESS_INSTALL_PLUGINS YES CACHE BOOL
  "Whether to install the Harness provided plugins")
set(ENABLE_TESTS OFF CACHE BOOL
  "Enable unit tests when building")
set(WITH_GMOCK "" CACHE PATH
  "Directory containing the Google Mock *source code* (with a CMakeLists.txt file)")

message(STATUS "Harness will install plugins in ${HARNESS_INSTALL_LIBRARY_DIR}/${HARNESS_NAME}")

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
  if(WITH_GMOCK)

    # There is a known gtest/gmock bug that surfaces with the gcc-6.x causing tests crashes:
    # https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=833450
    # We have a patch for it in the gmock we bundle but if the user wants to use
    # it's own gtest/gmock we need to prevent it if the gcc-6.x is used
    if ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      AND (CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "6.0" OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "6.0"))
      message(FATAL_ERROR "Parameter WITH_GMOCK is not supported for gcc-6 or greater."
        "You need to either disable the tests or use the bundled gmock (removing WITH_GMOCK parameter).")
    endif()

    set(_gmock_root ${WITH_GMOCK})
    set(_gtest_root ${WITH_GMOCK}/gtest)
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/gmock/CMakeLists.txt")
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/gtest/CMakeLists.txt")
      message(FATAL_ERROR "Cannot find GTest repository under ${CMAKE_CURRENT_SOURCE_DIR}/ext/gtest")
    endif()
    set(_gmock_root "${CMAKE_CURRENT_SOURCE_DIR}/ext/gmock")
    set(_gtest_root "${CMAKE_CURRENT_SOURCE_DIR}/ext/gtest")
  endif()

  if(NOT EXISTS "${_gmock_root}/CMakeLists.txt")
    message(FATAL_ERROR
      "Unable to find GMock source, not possible to build tests. Either "
      "disable tests with ENABLE_TESTS=no or download the source code "
      "for GMock (available at https://github.com/google/googlemock) and "
      "set WITH_GMOCK to the directory of the unpacked source code.")
  endif()

  message(STATUS "Found GMock source under ${_gmock_root}")
  add_subdirectory(${_gmock_root} ext/gmock)

  # Setting variables that are normally discovered using FindXXX.cmake
  set(GTEST_INCLUDE_DIRS ${_gtest_root}/include)
  set(GTEST_LIBRARIES gtest)
  set(GTEST_MAIN_LIBRARIES gtest_main)
  set(GTEST_BOTH_LIBRARIES ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})

  set(GMOCK_INCLUDE_DIRS ${_gmock_root}/include)
  set(GMOCK_LIBRARIES gmock)
  set(GMOCK_MAIN_LIBRARIES gmock_main)
  set(GMOCK_BOTH_LIBRARIES ${GMOCK_LIBRARIES} ${GMOCK_MAIN_LIBRARIES})

  set(TEST_LIBRARIES ${GMOCK_BOTH_LIBRARIES} ${GTEST_BOTH_LIBRARIES})

  # Since GMock and GTest do not set
  # INTERFACE_SYSTEM_INCLUDE_DIRECTORIES, we do that here. This means
  # that any targets that reference one of these libraries will
  # "automatically" have the include directories for these libraries
  # added to their build flags.  We cannot use "SYSTEM" since that is
  # not available in 2.8.9 (it was introduced in 2.8.12).
  target_include_directories(gmock PUBLIC ${GMOCK_INCLUDE_DIRS})
  target_include_directories(gmock_main PUBLIC ${GMOCK_INCLUDE_DIRS})
  target_include_directories(gtest PUBLIC ${GTEST_INCLUDE_DIRS})
  target_include_directories(gtest_main PUBLIC ${GTEST_INCLUDE_DIRS})
endif()

# Basic variables
set(HARNESS_NAME "harness"
  CACHE STRING "Name of Harness")
set(INSTALL_INCLUDE_DIR "include/mysql/harness")
set(HARNESS_INSTALL_INCLUDE_PREFIX "include/mysql")
set(HARNESS_INSTALL_PROGRAM YES
  CACHE BOOL "Whether to install the Harness main program")
set(HARNESS_INSTALL_PLUGINS YES
  CACHE BOOL "Whether to install the Harness provided plugins")
if(WIN32)
  foreach(config_ ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config_} config__)
    set(HARNESS_PLUGIN_OUTPUT_DIRECTORY_${config__} ${STAGE_DIR}/${config_}/lib
      CACHE STRING "Output directory for plugins ${config_} build")
  endforeach()
endif()
set(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib
  CACHE STRING "Output directory for plugins")
set(ENABLE_TESTS OFF
  CACHE BOOL "Enable unit tests when building")

include(CPack)
include(Plugin)
include(Arch)
include(Docs)
include(Check)

# Check for basic functions
find_package(Threads REQUIRED)

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
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Wpedantic")
  if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_COMPILER_IS_GNUCXX VERSION_LESS "4.9")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-write-strings")
  endif()
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
  if("${MSVC_VERSION}" VERSION_LESS 1800)
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER} is too old; need at least MSVC 12.0")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /TP")
else()
  message(FATAL_ERROR "You need C++11 support, but ${CMAKE_CXX_COMPILER} does not have that.")
endif()

# Platform specifics
if(WIN32)
  include(CheckIncludeFileCXX)
  check_include_file_cxx("shlwapi.h" Shlwapi_FOUND)
  if(Shlwapi_FOUND)
    set(SHLWAPI_LIBRARIES "shlwapi.dll")
  else()
    message(FATAL_ERROR "Shlwapi library not found")
  endif()
else()
  set(SHLWAPI_LIBRARIES)
endif()

# Check for basic functions
find_package(Threads REQUIRED)

if(ENABLE_TESTS)
  enable_testing()
  add_subdirectory(shared)
endif()

add_subdirectory(harness)
add_subdirectory(plugins)
