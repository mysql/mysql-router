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

# The sources are re-built to test the fabric cache implementation with
# a store layer that can supply the metadata without an actual connection
# to the fabric node. In this case, mock_fabric.cc implements the metadata
# interface without implementing a connection to the underlying fabric node.
# The MockFabric class does the functionality of the Fabric class.
set(FABRIC_CACHE_TESTS_HELPER
    ${CMAKE_SOURCE_DIR}/src/fabric_cache/src/fabric.cc
    ${CMAKE_SOURCE_DIR}/src/fabric_cache/src/fabric_cache.cc
    ${CMAKE_SOURCE_DIR}/src/fabric_cache/src/utils.cc
    ${CMAKE_SOURCE_DIR}/src/fabric_cache/src/cache_api.cc
    ${CMAKE_SOURCE_DIR}/src/fabric_cache/tests/helper/mock_fabric.cc
    ${CMAKE_SOURCE_DIR}/src/fabric_cache/tests/helper/mock_fabric_factory.cc
)

set(include_dirs
  ${CMAKE_SOURCE_DIR}/mysql_harness/plugins/logger/include
  ${CMAKE_SOURCE_DIR}/src/fabric_cache/include
  ${CMAKE_SOURCE_DIR}/src/fabric_cache/src
  ${CMAKE_SOURCE_DIR}/src/fabric_cache/tests/helper
  )

# We do not link to the fabric cache libraries since the sources are
# already built as part of the test libraries.
add_library(fabric_cache_tests SHARED ${FABRIC_CACHE_TESTS_HELPER})
target_link_libraries(fabric_cache_tests router_lib logger ${MySQL_LIBRARIES})
set_target_properties(fabric_cache_tests PROPERTIES
                      LIBRARY_OUTPUT_DIRECTORY
                      ${STAGE_DIR}/lib)
target_include_directories(fabric_cache_tests PRIVATE ${include_dirs})

add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}
             MODULE "fabric_cache"
             LIB_DEPENDS fabric_cache_tests
             INCLUDE_DIRS
             ${CMAKE_SOURCE_DIR}/mysql_harness/plugins/logger/include
             ${CMAKE_SOURCE_DIR}/src/fabric_cache/include
             ${CMAKE_SOURCE_DIR}/src/fabric_cache/src
             ${CMAKE_SOURCE_DIR}/src/fabric_cache/tests/helper
)