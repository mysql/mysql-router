# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

include_directories(../include ../src ../../../tests/helpers)

add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}
  MODULE "router"
  ENVIRONMENT "MYSQL_ROUTER_HOME=${MySQLRouter_BINARY_STAGE_DIR}/etc/"
  LIB_DEPENDS test-helpers
  INCLUDE_DIRS
    ${MySQLRouter_SOURCE_DIR}/mysql_harness/shared/include
    ${GTEST_INCLUDE_DIRS})

add_test_dir(issues MODULE issues
  INCLUDE_DIRS
  LIB_DEPENDS test-helpers
  ${GTEST_INCLUDE_DIRS})

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # silence undefined use of macro-vars in gtest.
  # we can't use #pragma's due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431 to
  # supress it locally.
  set_target_properties(
    test_router_uri
    PROPERTIES
    COMPILE_FLAGS "-Wno-undef")
endif()

file(GLOB config_files ${CMAKE_CURRENT_SOURCE_DIR}/*.conf ${CMAKE_CURRENT_SOURCE_DIR}/*.ini)
if(WIN32)
  foreach(conf ${CMAKE_CONFIGURATION_TYPES})
    foreach(config_file ${config_files})
      add_custom_command(TARGET test_router_mysqlrouter_app
        COMMAND ${CMAKE_COMMAND} -E copy ${config_file} ${MySQLRouter_BINARY_STAGE_DIR}/${conf}/etc/
        COMMENT "Copying Router test data: ${config_file}"
      )
    endforeach()
  endforeach()
else()
  foreach(config_file ${config_files})
    add_custom_command(TARGET test_router_mysqlrouter_app
      COMMAND ${CMAKE_COMMAND} -E copy ${config_file} ${MySQLRouter_BINARY_STAGE_DIR}/etc/
      COMMENT "Copying Router test data: ${config_file}"
    )
  endforeach()
endif()
