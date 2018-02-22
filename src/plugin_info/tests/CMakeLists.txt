# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

set(PLUGIN_INFO_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/library_file.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin_info_app.cc
)

# Implementation files that include RapidJSON headers.
# Used to supress warnings for those.
set(json_sources ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin.cc)
check_cxx_compiler_flag("-Wshadow" HAVE_SHADOW)
if(HAVE_SHADOW)
  add_compile_flags(${json_sources} COMPILE_FLAGS "-Wno-shadow")
endif()
check_cxx_compiler_flag("-Werror" HAVE_ERROR)
IF(HAVE_ERROR)
  add_compile_flags(${json_sources} COMPILE_FLAGS "-Wno-error")
ENDIF()
check_cxx_compiler_flag("-Wconversion" HAVE_CONVERSION)
IF(HAVE_CONVERSION)
  add_compile_flags(${json_sources} COMPILE_FLAGS "-Wno-conversion")
ENDIF()
check_cxx_compiler_flag("-Wpedantic" HAVE_PEDANTIC)
IF(HAVE_PEDANTIC)
  add_compile_flags(${json_sources} COMPILE_FLAGS "-Wno-pedantic")
ENDIF()


if(EXISTS "${CMAKE_SOURCE_DIR}/extra/rapidjson/")
  # prefer server-side rapidjson
  SET(RAPIDJSON_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/extra/rapidjson/include/")
else()
  SET(RAPIDJSON_INCLUDE_DIRS "${MySQLRouter_SOURCE_DIR}/ext/rapidjson/include/")
endif()

set(include_dirs
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/
  ${RAPIDJSON_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/harness/include
  )

if(NOT WIN32)
  add_library(plugin_info_tests SHARED ${PLUGIN_INFO_SOURCES})
else()
  add_library(plugin_info_tests STATIC ${PLUGIN_INFO_SOURCES})
endif()

target_link_libraries(plugin_info_tests harness-library)

set_target_properties(plugin_info_tests PROPERTIES
                      LIBRARY_OUTPUT_DIRECTORY
                      ${MySQLRouter_BINARY_STAGE_DIR}/lib)


target_include_directories(plugin_info_tests PRIVATE ${include_dirs})

add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}
             MODULE "mysqlrouter_plugin_info"
             LIB_DEPENDS
               plugin_info_tests
             INCLUDE_DIRS
               ${include_dirs}
)
