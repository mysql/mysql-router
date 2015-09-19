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

include_directories(../include ../src ../../../tests/helpers)

add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}
  MODULE "router"
  ENVIRONMENT "MYSQL_ROUTER_HOME=${STAGE_DIR}/etc/")

ADD_TEST_DIR(issues MODULE issues)

file(GLOB config_files ${CMAKE_CURRENT_SOURCE_DIR}/*.ini)
foreach(config_file ${config_files})
  add_custom_command(TARGET test_router_mysqlrouter_app
            COMMAND ${CMAKE_COMMAND} -E copy ${config_file} ${STAGE_DIR}/etc/
            COMMENT "Copying Router test data: ${config_file}"
  )
endforeach()