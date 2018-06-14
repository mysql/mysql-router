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

# Basic variables
set(HARNESS_NAME "harness" CACHE STRING
  "Name of Harness")

set(HARNESS_INSTALL_LIBRARY_DIR "${INSTALL_LIBDIR}"
    CACHE PATH "Installation directory for Harness libraries")
set(HARNESS_INSTALL_BIN_DIR "bin" CACHE PATH
    "Installation directory for Harness main binaries")
set(ENABLE_HARNESS_PROGRAM YES CACHE BOOL
  "Whether to build and install the Harness main program")
set(HARNESS_INSTALL_PLUGINS YES CACHE BOOL
  "Whether to install the Harness provided plugins")

message(STATUS "Harness will install plugins in ${HARNESS_INSTALL_LIBRARY_DIR}/${HARNESS_NAME}")

# Basic variables
set(HARNESS_NAME "harness"
  CACHE STRING "Name of Harness")
set(INSTALL_INCLUDE_DIR "include/mysql/harness")
set(HARNESS_INSTALL_INCLUDE_PREFIX "include/mysql")
set(HARNESS_INSTALL_PROGRAM YES
  CACHE BOOL "Whether to install the Harness main program")
set(HARNESS_INSTALL_PLUGINS YES
  CACHE BOOL "Whether to install the Harness provided plugins")
if(NOT CMAKE_CFG_INTDIR STREQUAL ".")
  foreach(config_ ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config_} config__)
    set(HARNESS_PLUGIN_OUTPUT_DIRECTORY_${config__} ${MySQLRouter_BINARY_STAGE_DIR}/${config_}/lib
      CACHE STRING "Output directory for plugins ${config_} build")
  endforeach()
endif()
set(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${MySQLRouter_BINARY_STAGE_DIR}/lib/${HARNESS_NAME}
  CACHE STRING "Output directory for plugins")
set(ENABLE_TESTS OFF
  CACHE BOOL "Enable unit tests when building")

include(Plugin)

# Check for C++ 11 support
include(CheckCXXCompilerFlag)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
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
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -Wconversion")
  if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_COMPILER_IS_GNUCXX VERSION_LESS "4.9")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-write-strings")
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  if(MSVC_VERSION VERSION_LESS 1800)
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER} is too old; need at least MSVC 12.0")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /TP")
  add_definitions(-DWIN32_LEAN_AND_MEAN)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "SunPro")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.13.0)
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION} is too old; need at least SunPro 5.13.0 (aka Oracle Developer Studio 12.4)")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
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

if(ENABLE_TESTS)
  enable_testing()
  add_subdirectory(shared)
endif()

include(${MySQLRouter_SOURCE_DIR}/cmake/compile_flags.cmake)
include(GenerateExportHeader)

set(MY_SSL_SOURCE_DIR ${MySQLRouter_SOURCE_DIR}/src/router/src/common)
set(MY_SSL_INCLUDE_DIRS ${MySQLRouter_SOURCE_DIR}/src/router/include
                        ${SSL_INCLUDE_DIRS})

set(harness_source
  ${MY_SSL_SOURCE_DIR}/my_aes.cc
  src/loader.cc src/utilities.cc src/config_parser.cc src/designator.cc
  src/loader_config.cc
  src/common.cc  src/filesystem.cc
  src/arg_handler.cc
  src/dim.cc
  src/hostname_validator.cc
  src/mysql_router_thread.cc
  src/process_launcher.cc
  src/logging/handler.cc
  src/logging/logger.cc
  src/logging/logging.cc
  src/logging/registry.cc
  src/random_generator.cc
  src/socket_operations.cc
  src/tcp_address.cc
  src/keyring/keyring_manager.cc
  src/keyring/keyring_memory.cc
  src/keyring/keyring_file.cc
  src/networking/ip_address.cc
  src/networking/ipv4_address.cc
  src/networking/ipv6_address.cc
  src/networking/resolver.cc)

if(WITH_SSL STREQUAL "bundled")
  set(MY_SSL_IMPL ${MY_SSL_SOURCE_DIR}/my_aes_yassl.cc)
else()
  set(MY_SSL_IMPL ${MY_SSL_SOURCE_DIR}/my_aes_openssl.cc)
endif()

set(harness_source ${harness_source} ${MY_SSL_IMPL})

# Disable warnings from 3rd party code that we have no control over.
if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  add_compile_flags(${MY_SSL_SOURCE_DIR}/my_aes.cc ${MY_SSL_IMPL} COMPILE_FLAGS
    -Wno-sign-conversion
    -Wno-unused-parameter
    -Wno-conversion)
endif()

if(WIN32)
  list(APPEND harness_source
    src/filesystem-windows.cc src/utilities-windows.cc src/loader-windows.cc)
else()
  list(APPEND harness_source
    src/filesystem-posix.cc src/utilities-posix.cc src/loader-posix.cc)
endif()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/mysql/harness
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${MySQLRouter_BINARY_DIR}/include
  ${MY_SSL_INCLUDE_DIRS})

if(WIN32)
  set(WINSOCK_LIBRARIES Ws2_32.lib)
endif()
set(common_libraries ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT}
                     ${SHLWAPI_LIBRARIES} ${WINSOCK_LIBRARIES} ${SSL_LIBRARIES})

configure_file(plugin.h.in ${MySQLRouter_BINARY_DIR}/${INSTALL_INCLUDE_DIR}/plugin.h
  ESCAPE_QUOTES @ONLY)

# create harness library - static version
add_library(harness-archive STATIC ${harness_source})
target_link_libraries(harness-archive ${common_libraries})
target_include_directories(harness-archive PUBLIC include)
if(WIN32)
  set_target_properties(harness-archive PROPERTIES
    COMPILE_FLAGS -DHARNESS_STATIC_DEFINE)
endif()
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC harness-archive "-pthread")
endif()

# create harness library - dynamic version
add_library(harness-library SHARED ${harness_source})
target_link_libraries(harness-library ${common_libraries})
target_include_directories(harness-library PUBLIC include)

generate_export_header(harness-library
  BASE_NAME HARNESS
  EXPORT_FILE_NAME ${MySQLRouter_BINARY_DIR}/include/harness_export.h)

if(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
  target_link_libraries(harness-library -lnsl -lsocket)
  target_link_libraries(harness-archive -lnsl -lsocket)
endif()

if(NOT WIN32)
  set_target_properties(harness-archive harness-library PROPERTIES
    OUTPUT_NAME "mysqlharness"
    PREFIX "lib"
    SOVERSION 1)
else()
  set_target_properties(harness-archive PROPERTIES
    OUTPUT_NAME "mysqlharness_a"
    PREFIX "lib"
    SOVERSION 1)
endif()

# Fix install path in stage dir, so that files get to
# stage/${configuration_type}/{bin,etc,lib} instead of
# stage/{bin,etc,lib}/${configuration_type}
if (NOT CMAKE_CFG_INTDIR STREQUAL ".")
  foreach(config_ ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config_} config_)
    set_target_properties(harness-library PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${config_} ${MySQLRouter_BINARY_DIR}/stage/${config_}/bin
      LIBRARY_OUTPUT_DIRECTORY_${config_} ${MySQLRouter_BINARY_DIR}/stage/${config_}/lib
    )
  endforeach()
else()
  set_target_properties(harness-library PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${MySQLRouter_BINARY_DIR}/stage/lib
    RUNTIME_OUTPUT_DIRECTORY ${MySQLRouter_BINARY_DIR}/stage/bin)
endif()

# don't install headers until a) a final destination is found and b) API is stable
# file(GLOB harness_headers include/*.h)
# install(FILES ${harness_headers}
#   DESTINATION ${HARNESS_INSTALL_INCLUDE_PREFIX}/${HARNESS_NAME})
if(NOT WIN32)
  install(TARGETS harness-archive harness-library
        LIBRARY DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR}
        ARCHIVE DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR})
else()
  install(TARGETS harness-archive
        ARCHIVE DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR})
  install(TARGETS harness-library
        RUNTIME DESTINATION ${HARNESS_INSTALL_BIN_DIR}
        LIBRARY DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR})
  install(FILES $<TARGET_PDB_FILE:harness-library>
        DESTINATION ${HARNESS_INSTALL_BIN_DIR})
endif()

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
