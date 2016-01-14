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

include_directories(
  ../include
  ../src
  ${CMAKE_SOURCE_DIR}/src/fabric_cache/include
  ${CMAKE_SOURCE_DIR}/src/mysql_protocol/include
  ../../../tests/helpers
)

add_library(routing_tests SHARED ${ROUTING_SOURCE_FILES})
target_link_libraries(routing_tests routertest_helpers logger router_lib fabric_cache mysql_protocol)
set_target_properties(routing_tests PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${STAGE_DIR}/lib)
target_include_directories(routing PRIVATE ${include_dirs})

add_library(routing_plugin_tests SHARED ${ROUTING_PLUGIN_SOURCE_FILES})
target_link_libraries(routing_plugin_tests routing_tests routertest_helpers logger router_lib fabric_cache)
set_target_properties(routing_plugin_tests PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${STAGE_DIR}/lib)
target_include_directories(routing_plugin_tests PRIVATE ${include_dirs})

if(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
  target_link_libraries(routing_tests -lnsl -lsocket)
  target_link_libraries(routing_plugin_tests -lnsl -lsocket)
endif()


add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}
  MODULE "routing"
  LIB_DEPENDS routing_tests
  ENVIRONMENT "MYSQL_ROUTER_HOME=${STAGE_DIR}/etc/")

add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}/plugin
  MODULE "routing"
  LIB_DEPENDS routing_tests routing_plugin_tests
  ENVIRONMENT "MYSQL_ROUTER_HOME=${STAGE_DIR}/etc/"
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_SOURCE_DIR}/tests/helpers)

ADD_TEST_DIR(issues MODULE issues
  LIB_DEPENDS routing_tests routing_plugin_tests
  INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_SOURCE_DIR}/tests/helpers)

set(RUNNING_MYSQL_SERVER "127.0.0.1:3306")
configure_file(data/1route.ini.in ${STAGE_DIR}/etc/routing_1route.ini)

