# Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
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

# The sources are re-built to test the metadata cache implementation with
# a store layer that can supply the metadata without an actual connection
# to the metadata node. In this case, mock_metadata.cc implements the metadata
# interface without implementing a connection to the underlying metadata node.
# The MockMetadata class does the functionality of the Metadata class.

set(METADATA_CACHE_TESTS_HELPER
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/src/cluster_metadata.cc
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/src/metadata_cache.cc
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/src/cache_api.cc
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/src/plugin_config.cc
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/src/group_replication_metadata.cc
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/tests/helper/mock_metadata.cc
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/tests/helper/mock_metadata_factory.cc
)


set(include_dirs
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/include
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/src
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/tests/helper
  ${PROJECT_SOURCE_DIR}/tests/helpers
  )

# We do not link to the metadata cache libraries since the sources are
# already built as part of the test libraries.
if(NOT WIN32)
  add_library(metadata_cache_tests SHARED ${METADATA_CACHE_TESTS_HELPER})
  target_link_libraries(metadata_cache_tests router_lib ${MySQL_LIBRARIES})
else()
  add_library(metadata_cache_tests STATIC ${METADATA_CACHE_TESTS_HELPER})
  target_link_libraries(metadata_cache_tests router_lib metadata_cache ${MySQL_LIBRARIES})
  target_compile_definitions(metadata_cache_tests PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
  target_compile_definitions(metadata_cache_tests PRIVATE -Dmetadata_cache_tests_DEFINE_STATIC=1)
endif()

set_target_properties(metadata_cache_tests PROPERTIES
                      LIBRARY_OUTPUT_DIRECTORY
                      ${MySQLRouter_BINARY_STAGE_DIR}/lib)


target_include_directories(metadata_cache_tests PRIVATE ${include_dirs})

add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}
             MODULE "metadata_cache"
             LIB_DEPENDS
               metadata_cache_tests
               test-helpers
             INCLUDE_DIRS
               ${PROJECT_SOURCE_DIR}/src/metadata_cache/include
               ${PROJECT_SOURCE_DIR}/src/metadata_cache/src
               ${PROJECT_SOURCE_DIR}/src/metadata_cache/tests/helper
               ${PROJECT_SOURCE_DIR}/tests/helpers
               ${PROJECT_SOURCE_DIR}/src/harness/shared/include
)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # silence undefined use of macro-vars in gtest.
  # we can't use #pragma's due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431 to
  # supress it locally.
  set_target_properties(
    test_metadata_cache_plugin_config
    PROPERTIES
    COMPILE_FLAGS "-Wno-undef -Wno-conversion")
endif()



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
