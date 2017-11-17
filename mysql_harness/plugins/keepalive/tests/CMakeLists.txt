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

enable_testing()

include_directories(${GTEST_INCLUDE_DIRS} ${GMOCK_INCLUDE_DIRS})

SET(TESTS
  test_plugin_keepalive.cc
)

foreach(TEST ${TESTS})
  add_test_file(${TEST} MODULE harness
    INCLUDE_DIRS ${MySQLRouter_SOURCE_DIR}/mysql_harness/shared/include/
    LIB_DEPENDS test-helpers)
endforeach()


create_harness_test_directory_post_build(test_harness_plugin_keepalive keepalive)
configure_harness_test_file(data/keepalive.cfg.in data/keepalive.cfg)
