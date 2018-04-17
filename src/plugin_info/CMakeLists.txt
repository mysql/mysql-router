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

file(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc)
include_directories(${CMAKE_BINARY_DIR}/include)
add_executable(mysqlrouter_plugin_info ${sources})

# Implementation files that include RapidJSON headers.
# Used to supress warnings for those.
set(json_sources src/plugin.cc)
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

set(common_libraries)
if(WIN32)
  # we nedd harness for Path only on Windows
  list(APPEND common_libraries harness-library)
endif()

target_link_libraries(mysqlrouter_plugin_info ${common_libraries} ${CMAKE_DL_LIBS})

if(EXISTS "${CMAKE_SOURCE_DIR}/extra/rapidjson/")
  # prefer server-side rapidjson
  SET(RAPIDJSON_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/extra/rapidjson/include/")
else()
  SET(RAPIDJSON_INCLUDE_DIRS "${MySQLRouter_SOURCE_DIR}/ext/rapidjson/include/")
endif()

target_include_directories(mysqlrouter_plugin_info PUBLIC ${RAPIDJSON_INCLUDE_DIRS})

set_target_output_directory(mysqlrouter_plugin_info RUNTIME_OUTPUT_DIRECTORY bin)

install(TARGETS mysqlrouter_plugin_info
        RUNTIME DESTINATION ${INSTALL_BINDIR})

if(WIN32)
  install(FILES $<TARGET_PDB_FILE:mysqlrouter_plugin_info>
        DESTINATION ${INSTALL_BINDIR})
endif()

if(ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests/)
endif()
