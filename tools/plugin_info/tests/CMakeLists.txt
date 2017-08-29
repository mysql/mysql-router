# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

set(include_dirs
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/
  ${CMAKE_SOURCE_DIR}/ext/rapidjson/include/
  ${CMAKE_SOURCE_DIR}/mysql_harness/harness/include
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
