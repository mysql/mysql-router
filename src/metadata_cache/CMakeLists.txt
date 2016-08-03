# Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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

set(METADATA_CACHE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/metadata_factory.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/farm_metadata.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/metadata_cache.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cache_api.cc
)

include_directories(
  ../router/include
  include/
  src/
  ${MySQL_INCLUDE_DIRS}
)

add_harness_plugin(metadata_cache SOURCES
  src/metadata_cache_plugin.cc
  src/plugin_config.cc
  ${METADATA_CACHE_SOURCES}
  REQUIRES logger router_lib)

target_link_libraries(metadata_cache ${MySQL_LIBRARIES})
file(GLOB metadata_cache_headers include/mysqlrouter/*.h)
install(FILES ${metadata_cache_headers}
  DESTINATION include/mysql/${HARNESS_NAME})

if(ENABLE_TESTS)
    add_subdirectory(tests/)
endif()

if(BUILD_SAMPLES)
  add_executable(metadatacache_test metadata_cache_dev.cc)
  target_link_libraries(metadatacache_test metadata_cache ${MySQL_LIBRARIES})
  if(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
    target_link_libraries(metadatacache_test -lnsl -lsocket)
  endif()
  set_target_properties(metadatacache_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${STAGE_DIR}/bin)
endif(BUILD_SAMPLES)
