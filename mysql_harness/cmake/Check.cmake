# Copyright (c), 2016, Oracle and/or its affiliates. All rights reserved.
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

find_program(CPPLINT NAMES "cpplint" "cpplint.py")

if(CPPLINT)
  message(STATUS "Cpplint found as ${CPPLINT}, creating 'check' target")
  file(GLOB_RECURSE _files
    ${CMAKE_SOURCE_DIR}/harness/*.cc
    ${CMAKE_SOURCE_DIR}/harness/*.h
    ${CMAKE_SOURCE_DIR}/plugins/*.cc
    ${CMAKE_SOURCE_DIR}/plugins/*.h)
  add_custom_target(check
    COMMENT "Run lint checks on all source files in tree"
    COMMAND ${CPPLINT} ${CPPLINT_FLAGS} ${_files})
else()
  message(STATUS "No cpplint found, not creating 'check' target")
endif()
