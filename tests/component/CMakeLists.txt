# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

add_test_dir(${CMAKE_CURRENT_SOURCE_DIR} MODULE "component")

add_subdirectory(mysqld_mock)

# When system deployment bootstrap is executed, files are
# created in locations specified by deployment layout.
# Since component tests may not have access to directories defined
# in install_layout.cmake, system bootstrap deployment tests are
# executed only for STANDALONE layout, are are not executed on Windows.
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" OR NOT INSTALL_LAYOUT STREQUAL "STANDALONE")
  add_definitions(-DSKIP_BOOTSTRAP_SYSTEM_DEPLOYMENT_TESTS)
endif()