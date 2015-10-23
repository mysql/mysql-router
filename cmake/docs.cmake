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

find_package(Doxygen)

if (DOXYGEN_FOUND)
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/doc)

  configure_file(doc/doxygen.cfg.in ${CMAKE_BINARY_DIR}/doc/doxgen.cfg @ONLY)

  foreach(f router_footer.html router_header.html router_doxygen.css)
    file(COPY ${CMAKE_SOURCE_DIR}/doc/${f} DESTINATION ${CMAKE_BINARY_DIR}/doc)
  endforeach()

  add_custom_target(doc ${DOXYGEN_EXECUTABLE} doc/doxgen.cfg
    COMMAND pwd
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generate MySQL Router developer documentation"
    VERBATIM)
endif()
