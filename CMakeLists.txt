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

cmake_minimum_required(VERSION 2.8.12)

include(cmake/version.cmake)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

if(${CMAKE_VERSION} VERSION_GREATER "3.0")
  cmake_policy(SET CMP0042 NEW)
  cmake_policy(SET CMP0046 NEW)
  cmake_policy(SET CMP0048 NEW)
  if(${CMAKE_VERSION} VERSION_GREATER "3.1")
    cmake_policy(SET CMP0054 NEW)
  endif()
  project("MySQLRouter" VERSION ${PROJECT_VERSION_TEXT} LANGUAGES C CXX)
else()
  project("MySQLRouter")
endif()

SET(MySQLRouter_BINARY_STAGE_DIR ${MySQLRouter_BINARY_DIR}/stage CACHE INTERNAL "STAGE_DIR")

if(EXISTS "${CMAKE_SOURCE_DIR}/extra/rapidjson/")
  # prefer server-side rapidjson
  SET(RAPIDJSON_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/extra/rapidjson/include/")
else()
  SET(RAPIDJSON_INCLUDE_DIRS "${MySQLRouter_SOURCE_DIR}/ext/rapidjson/include/")
endif()

IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  # disable rapidjson optimisation on Solaris as the it breaks
  # shared objects that build with -fPIC
  ADD_DEFINITIONS(-DRAPIDJSON_48BITPOINTER_OPTIMIZATION=0)
ENDIF()

IF(EXISTS "${CMAKE_SOURCE_DIR}/rapid/plugin/x/protocol/mysqlx.proto")
  # use the server's protobuf files, we they exist
  SET(PROTOBUF_MYSQLX_DIR "${CMAKE_SOURCE_DIR}/rapid/plugin/x/protocol/")
ELSE()
  SET(PROTOBUF_MYSQLX_DIR "${MySQLRouter_SOURCE_DIR}/src/x_protocol/proto/")
ENDIF()

find_package(LibEvent2 2.0 MODULE REQUIRED)

include(cmake/settings.cmake)

if(WITH_UNIT_TESTS)
  set(ENABLE_TESTS 1)
endif()

# Load Internal
if(EXISTS "${PROJECT_SOURCE_DIR}/internal/")
  message(STATUS "Loading internal repository")
  add_subdirectory(internal)
endif()

# SSL
# Add bundled yassl/taocrypt or system openssl.
include(cmake/ssl.cmake)
if(NOT WITH_SSL)
  set(WITH_SSL bundled)
endif()
MYSQL_CHECK_SSL()
MESSAGE(STATUS "SSL_LIBRARIES:    ${SSL_LIBRARIES}")
MESSAGE(STATUS "SSL_DEFINES:      ${SSL_DEFINES}")
MESSAGE(STATUS "SSL_INCLUDE_DIRS: ${SSL_INCLUDE_DIRS}")

IF(UNIX)
  OPTION(WITH_VALGRIND "Valgrind instrumentation" OFF)
ENDIF()

# Required tools, libraries, etc..
include(cmake/testing.cmake)  # does not enable testing
include(cmake/platform.cmake)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/install_layout.cmake")
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/install_layout.cmake)
else()
  include(cmake/install_layout.cmake)  # needs ARCH_64BIT
endif()
include(cmake/configure.cmake)
include(Coverage)
# Harness before Compiler.cmake (compiler flags), after configure.cmake because of compiler flags
include(cmake/compiler.cmake)
find_package(Threads REQUIRED)
include(cmake/docs.cmake)
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/include/mysql.h")
  # if we are built as part of the server-tree, don't initiate our own packaging
  include(cmake/packaging.cmake)
endif()

find_package(MySQL 5.5 REQUIRED)
include_directories(${PROJECT_BINARY_DIR}/include)

include(cmake/compile_flags.cmake)
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/include/mysql.h")
  # if we are build as part of the server-tree, use their protobuf
  include(cmake/protobuf.cmake)
  MYSQL_CHECK_PROTOBUF()
endif()

# Enable testing
if(ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Load all modules, including plugins
add_subdirectory(src)
