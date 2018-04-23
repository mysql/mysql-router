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
  if(TARGET gmock)
    # don't build gmock, if the parent already built it

    # copying from unittest/gunit/CMakeFiles.txt
    # this should all be global-variables or a cmake/ file
    if(NOT DOWNLOAD_ROOT)
      set(DOWNLOAD_ROOT ${CMAKE_SOURCE_DIR}/source_downloads)
    endif()

    # We want googletest version 1.8, which also contains googlemock.
    set(GMOCK_PACKAGE_NAME "release-1.8.0")

    if(DEFINED ENV{WITH_GMOCK} AND NOT DEFINED WITH_GMOCK)
      file(TO_CMAKE_PATH "$ENV{WITH_GMOCK}" WITH_GMOCK)
    ENDIF()

    if(LOCAL_GMOCK_ZIP
       AND NOT ${LOCAL_GMOCK_ZIP} MATCHES ".*${GMOCK_PACKAGE_NAME}\\.zip")
     set(LOCAL_GMOCK_ZIP 0)
    endif()

    if(WITH_GMOCK)
      ## Did we get a full path name, including file name?
      if(${WITH_GMOCK} MATCHES ".*\\.zip")
        GET_FILENAME_COMPONENT(GMOCK_DIR ${WITH_GMOCK} PATH)
        GET_FILENAME_COMPONENT(GMOCK_ZIP ${WITH_GMOCK} NAME)
        FIND_FILE(LOCAL_GMOCK_ZIP
                  NAMES ${GMOCK_ZIP}
                  PATHS ${GMOCK_DIR}
                  NO_DEFAULT_PATH
                 )
      else()
        ## Did we get a path name to the directory of the .zip file?
        ## Check for both release-x.y.z.zip and googletest-release-x.y.z.zip
        FIND_FILE(LOCAL_GMOCK_ZIP
                  NAMES "${GMOCK_PACKAGE_NAME}.zip" "googletest-${GMOCK_PACKAGE_NAME}.zip"
                  PATHS ${WITH_GMOCK}
                  NO_DEFAULT_PATH
                  )
        ## If WITH_GMOCK is a directory, use it for download.
        set(DOWNLOAD_ROOT ${WITH_GMOCK})
      endif()
      MESSAGE(STATUS "Local gmock zip ${LOCAL_GMOCK_ZIP}")
    endif()

    set(GMOCK_SOURCE_DIR ${DOWNLOAD_ROOT}/googletest-${GMOCK_PACKAGE_NAME}/googlemock)
    set(GTEST_SOURCE_DIR ${DOWNLOAD_ROOT}/googletest-${GMOCK_PACKAGE_NAME}/googletest)

    # introduce some compat
    set(GTEST_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})

    ADD_LIBRARY(gmock_main STATIC ${GMOCK_SOURCE_DIR}/src/gmock_main.cc)
    target_link_libraries(gmock_main gmock)
    target_include_directories(gmock_main
      PUBLIC ${GMOCK_INCLUDE_DIRS})
    ADD_LIBRARY(gtest_main STATIC ${GTEST_SOURCE_DIR}/src/gtest_main.cc)
    target_include_directories(gtest_main
      PUBLIC ${GMOCK_INCLUDE_DIRS})

    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      set_target_properties(gtest_main gmock_main
        PROPERTIES
        COMPILE_FLAGS "-Wno-undef -Wno-conversion")
    endif()

    set(TEST_LIBRARIES gmock gtest gmock_main gtest_main)
  else()
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
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/ext/gmock/CMakeLists.txt")
      if(NOT EXISTS "${CMAKE_SOURCE_DIR}/ext/gtest/CMakeLists.txt")
        message(FATAL_ERROR "Cannot find GTest repository under ${CMAKE_SOURCE_DIR}/ext/gtest")
      endif()
      set(_gmock_root "${CMAKE_SOURCE_DIR}/ext/gmock")
      set(_gtest_root "${CMAKE_SOURCE_DIR}/ext/gtest")
    elseif(GMOCK_SOURCE_DIR)
      # means we are part of the server and GMOCK was downloaded
      set(_gmock_root ${GMOCK_SOURCE_DIR})
      set(_gtest_root ${GMOCK_SOURCE_DIR}/gtest)
    else()
      # means we are part of the server and GMOCK is missing
      # act as other server components, disable the tests
      SET (ENABLE_TESTS 0)
      SET (ENABLE_TESTS 0 PARENT_SCOPE)
    endif()

    if (ENABLE_TESTS)
      if(NOT EXISTS "${_gmock_root}/CMakeLists.txt")
        message(WARNING
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

      if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

        set (comp_flags_ "-Wno-undef -Wno-missing-field-initializers")
        if(COMPILER_HAS_WARNING_MISSING_FORMAT_ATTRIBUTE)
          set(comp_flags_ "${comp_flags_} -Wno-missing-format-attribute")
        endif()

        set_target_properties(gtest gtest_main gmock gmock_main
          PROPERTIES
          COMPILE_FLAGS "${comp_flags_}")
      endif()
    endif()
  endif()
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
