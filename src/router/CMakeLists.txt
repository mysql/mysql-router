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
  include/
  ${CMAKE_SOURCE_DIR}/mysql_harness/plugins/logger/include
)

add_subdirectory(src/)
if(ENABLE_TESTS)
    add_subdirectory(tests/)
endif()

if(INSTALL_DOCDIR)
  install(FILES ${CMAKE_SOURCE_DIR}/README.txt DESTINATION ${INSTALL_DOCDIR})
  install(FILES ${CMAKE_SOURCE_DIR}/License.txt DESTINATION ${INSTALL_DOCDIR})
endif()

file(GLOB router_headers include/mysqlrouter/*.h)
install(FILES ${router_headers}
  DESTINATION "include/mysql/${HARNESS_NAME}")