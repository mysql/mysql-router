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
  REQUIRES router_lib)
target_include_directories(mysql_protocol PRIVATE ${include_dirs})

# don't install headers until a) a final destination is found and b) API is stable
# file(GLOB mysqlv10_headers include/mysqlrouter/*.h)
# install(FILES ${mysqlv10_headers}
#   DESTINATION "include/mysql/${HARNESS_NAME}")

# file(GLOB mysqlv10_headers include/mysqlrouter/mysql_protocol/*.h)
# install(FILES ${mysqlv10_headers}
#   DESTINATION "include/${HARNESS_NAME}/mysql_protocol")

if(ENABLE_TESTS)
  add_subdirectory(tests/)
endif()
