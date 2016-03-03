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

set(SOURCE_FILES
  src/handshake_packet.cc
  src/error_packet.cc
  src/base_packet.cc
  )

set(include_dirs
  ../src/router/include
  include
  )

add_harness_plugin(mysql_protocol
  SOURCES ${SOURCE_FILES}
  REQUIRES router_lib logger)
target_include_directories(mysql_protocol PRIVATE ${include_dirs})

file(GLOB mysqlv10_headers include/mysqlrouter/*.h)
install(FILES ${mysqlv10_headers}
  DESTINATION "include/mysql/${HARNESS_NAME}")

file(GLOB mysqlv10_headers include/mysqlrouter/mysql_protocol/*.h)
install(FILES ${mysqlv10_headers}
  DESTINATION "include/${HARNESS_NAME}/mysql_protocol")

if(ENABLE_TESTS)
  add_subdirectory(tests/)
endif()
