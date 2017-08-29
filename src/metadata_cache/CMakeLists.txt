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

set(METADATA_CACHE_SOURCES
  src/metadata_factory.cc
  src/cluster_metadata.cc
  src/metadata_cache.cc
  src/cache_api.cc
  src/group_replication_metadata.cc
)

include_directories(
  ../router/include
  include/
  src/
  ${MySQL_INCLUDE_DIRS}
)

add_definitions(${SSL_DEFINES})

add_harness_plugin(metadata_cache SOURCES
  src/metadata_cache_plugin.cc
  src/plugin_config.cc
  ${METADATA_CACHE_SOURCES}
  REQUIRES router_lib)

target_link_libraries(metadata_cache PRIVATE ${MySQL_LIBRARIES})
# don't install headers until a) a final destination is found and b) API is stable
# file(GLOB metadata_cache_headers include/mysqlrouter/*.h)
# install(FILES ${metadata_cache_headers}
#   DESTINATION include/mysql/${HARNESS_NAME})

if(ENABLE_TESTS)
    add_subdirectory(tests/)
endif()
