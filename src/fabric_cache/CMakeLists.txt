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

include_directories(
  ../router/include
  include/
  src/
  ${MySQL_INCLUDE_DIRS}
)

set(FABRIC_CACHE_SOURCES
  src/fabric_factory.cc
  src/fabric.cc
  src/fabric_cache.cc
  src/utils.cc
  src/cache_api.cc
)

add_harness_plugin(fabric_cache SOURCES
  src/fabric_cache_plugin.cc
  src/plugin_config.cc
  ${FABRIC_CACHE_SOURCES}
  REQUIRES logger router_lib)

target_link_libraries(fabric_cache ${MySQL_LIBRARIES})
file(GLOB fabric_cache_headers include/mysqlrouter/*.h)
install(FILES ${fabric_cache_headers}
  DESTINATION include/mysql/${HARNESS_NAME})

add_executable(cache_test fabric_cache_dev.cc)
target_link_libraries(cache_test fabric_cache ${MySQL_LIBRARIES})
if(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
  target_link_libraries(cache_test -lnsl -lsocket)
endif()
set_target_properties(cache_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${STAGE_DIR}/bin)
