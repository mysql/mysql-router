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

set(FABRIC_CACHE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/fabric_factory.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/fabric.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/fabric_cache.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cache_api.cc
)

include_directories(
  ../router/include
  include/
  src/
  ${MySQL_INCLUDE_DIRS}
  "${CMAKE_BINARY_DIR}/harness"
)

set(FABRIC_CACHE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/fabric_factory.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/fabric.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/fabric_cache.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cache_api.cc
)

if(WIN32)
  add_harness_plugin(fabric_cache SOURCES
    src/fabric_cache_plugin.cc
    src/plugin_config.cc
    ../router/src/windows/password_vault.cc
    ${FABRIC_CACHE_SOURCES}
    REQUIRES logger router_lib)
else()
  add_harness_plugin(fabric_cache SOURCES
    src/fabric_cache_plugin.cc
    src/plugin_config.cc
    ${FABRIC_CACHE_SOURCES}
    REQUIRES logger router_lib)
endif()

target_link_libraries(fabric_cache PUBLIC ${MySQL_LIBRARIES})
if(WITH_HARNESS)
  target_include_directories(fabric_cache PUBLIC "${CMAKE_BINARY_DIR}")
  target_include_directories(fabric_cache PUBLIC "${CMAKE_BINARY_DIR}/include/mysql/harness")
  target_include_directories(fabric_cache PUBLIC "${CMAKE_BINARY_DIR}/include")
endif()

file(GLOB fabric_cache_headers include/mysqlrouter/*.h)
install(FILES ${fabric_cache_headers}
  DESTINATION include/mysql/${HARNESS_NAME})

if(ENABLE_TESTS)
    add_subdirectory(tests/)
endif()

if(BUILD_SAMPLES)
  add_executable(cache_test fabric_cache_dev.cc)
  target_link_libraries(cache_test fabric_cache ${MySQL_LIBRARIES})
  if(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
    target_link_libraries(cache_test -lnsl -lsocket)
  endif()
  set_target_properties(cache_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${STAGE_DIR}/bin)
endif(BUILD_SAMPLES)
