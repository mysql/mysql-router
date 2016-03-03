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

include_directories(helpers
  ${CMAKE_SOURCE_DIR}/mysql_harness/harness/include
  )

add_library(routertest_helpers SHARED
  helpers/helper_logger.cc
  helpers/cmd_exec.cc
  helpers/router_test_helpers.cc
)
target_link_libraries(routertest_helpers harness-library)
set_target_properties(routertest_helpers PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${STAGE_DIR}/lib
  INCLUDE_DIRECTORIES
    "${CMAKE_SOURCE_DIR}/tests/helpers/;${CMAKE_SOURCE_DIR}/mysql_harness/harness/include"
)

foreach(test_dir legal tools)
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${test_dir})
        ADD_TEST_DIR(${CMAKE_CURRENT_SOURCE_DIR}/${test_dir} MODULE project)
    endif()
endforeach()

