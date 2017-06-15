# Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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

# The sources are re-built to test the metadata cache implementation with
# a store layer that can supply the metadata without an actual connection
# to the metadata node. In this case, mock_metadata.cc implements the metadata
# interface without implementing a connection to the underlying metadata node.
# The MockMetadata class does the functionality of the Metadata class.

set(METADATA_CACHE_TESTS_HELPER
  ${CMAKE_SOURCE_DIR}/src/metadata_cache/src/cluster_metadata.cc
  ${CMAKE_SOURCE_DIR}/src/metadata_cache/src/metadata_cache.cc
  ${CMAKE_SOURCE_DIR}/src/metadata_cache/src/cache_api.cc
  ${CMAKE_SOURCE_DIR}/src/metadata_cache/src/plugin_config.cc
  ${CMAKE_SOURCE_DIR}/src/metadata_cache/src/group_replication_metadata.cc
  ${CMAKE_SOURCE_DIR}/src/metadata_cache/tests/helper/mock_metadata.cc
  ${CMAKE_SOURCE_DIR}/src/metadata_cache/tests/helper/mock_metadata_factory.cc
)


set(include_dirs
  ${CMAKE_SOURCE_DIR}/mysql_harness/plugins/logger/include
  ${CMAKE_SOURCE_DIR}/src/metadata_cache/include
  ${CMAKE_SOURCE_DIR}/src/metadata_cache/src
  ${CMAKE_SOURCE_DIR}/src/metadata_cache/tests/helper
  ${CMAKE_SOURCE_DIR}/tests/helpers
  )

# We do not link to the metadata cache libraries since the sources are
# already built as part of the test libraries.
if(NOT WIN32)
  add_library(metadata_cache_tests SHARED ${METADATA_CACHE_TESTS_HELPER})
  target_link_libraries(metadata_cache_tests router_lib logger ${MySQL_LIBRARIES})
else()
  add_library(metadata_cache_tests STATIC ${METADATA_CACHE_TESTS_HELPER})
  target_link_libraries(metadata_cache_tests router_lib logger metadata_cache ${MySQL_LIBRARIES})
  target_compile_definitions(metadata_cache_tests PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
  target_compile_definitions(metadata_cache_tests PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
endif()

set_target_properties(metadata_cache_tests PROPERTIES
                      LIBRARY_OUTPUT_DIRECTORY
                      ${STAGE_DIR}/lib)


target_include_directories(metadata_cache_tests PRIVATE ${include_dirs})

add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}
             MODULE "metadata_cache"
             LIB_DEPENDS metadata_cache_tests
             INCLUDE_DIRS
             ${CMAKE_SOURCE_DIR}/mysql_harness/plugins/logger/include
             ${CMAKE_SOURCE_DIR}/src/metadata_cache/include
             ${CMAKE_SOURCE_DIR}/src/metadata_cache/src
             ${CMAKE_SOURCE_DIR}/src/metadata_cache/tests/helper
             ${CMAKE_SOURCE_DIR}/tests/helpers
)

target_compile_definitions(test_metadata_cache_cache_plugin PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
target_compile_definitions(test_metadata_cache_metadata_cache PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
target_compile_definitions(test_metadata_cache_cache_plugin PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
target_compile_definitions(test_metadata_cache_metadata_cache PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
target_compile_definitions(test_metadata_cache_metadata PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
target_compile_definitions(test_metadata_cache_metadata PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
target_compile_definitions(test_metadata_cache_failover PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
target_compile_definitions(test_metadata_cache_failover PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
target_compile_definitions(test_metadata_cache_plugin_config PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
target_compile_definitions(test_metadata_cache_plugin_config PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
