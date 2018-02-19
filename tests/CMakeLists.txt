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

include_directories(helpers
  ${PROJECT_SOURCE_DIR}/src/harness/include
  ${PROJECT_SOURCE_DIR}/src/router/include/mysqlrouter
  ${GTEST_INCLUDE_DIRS}
  )

set(helper_sources
  helpers/router_test_helpers.cc
  helpers/mysql_session_replayer.cc
  helpers/router_component_test.cc
  helpers/tcp_port_pool.cc
  helpers/script_generator.cc
)

if(WIN32)
  list(APPEND helper_sources helpers/cmd_exec-windows.cc)
  add_library(routertest_helpers STATIC ${helper_sources})
else()
  list(APPEND helper_sources helpers/cmd_exec.cc)
  add_library(routertest_helpers SHARED ${helper_sources})
endif()

target_link_libraries(routertest_helpers harness-library router_lib)

set(helpers_include_dirs
  ${PROJECT_SOURCE_DIR}/tests/helpers/
  ${PROJECT_SOURCE_DIR}/src/harness/include
  ${PROJECT_BINARY_DIR}/
  ${PROJECT_BINARY_DIR}/harness
  ${PROJECT_BINARY_DIR}/include
  ${GTEST_INCLUDE_DIRS}
  )

set_target_properties(routertest_helpers PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${MySQLRouter_BINARY_STAGE_DIR}/lib
  INCLUDE_DIRECTORIES "${helpers_include_dirs}"
)

IF(NOT GPL)
  ADD_DEFINITIONS(-DHAVE_LICENSE_COMMERCIAL)
ENDIF()

foreach(test_dir legal tools)
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${test_dir})
        add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}/${test_dir} MODULE project)
    endif()
endforeach()

add_subdirectory(component)
add_subdirectory(fuzzers)

add_definitions(-DCOMPONENT_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/component/data/")
