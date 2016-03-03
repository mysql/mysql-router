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

set(ROUTING_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mysql_routing.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/destination.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dest_fabric_cache.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dest_first_available.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/routing.cc
)

set(ROUTING_PLUGIN_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/routing_plugin.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin_config.cc
)

set(include_dirs
  ${CMAKE_SOURCE_DIR}/mysql_harness/plugins/logger/include
  ${CMAKE_SOURCE_DIR}/src/router/include
  ${CMAKE_SOURCE_DIR}/src/routing/include
  ${CMAKE_SOURCE_DIR}/src/fabric_cache/include
  ${CMAKE_SOURCE_DIR}/src/mysql_protocol/include
)

# The Plugin
add_harness_plugin(routing
        SOURCES ${ROUTING_PLUGIN_SOURCE_FILES} ${ROUTING_SOURCE_FILES}
        REQUIRES logger mysql_protocol)
target_include_directories(routing PRIVATE ${include_dirs})

if(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
  target_link_libraries(routing -lnsl -lsocket)
endif()

file(GLOB routing_headers include/mysqlrouter/*.h)
install(FILES ${routing_headers}
  DESTINATION "include/mysql/${HARNESS_NAME}")

if(ENABLE_TESTS)
  add_subdirectory(tests/)
endif()

