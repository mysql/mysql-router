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

# Find the MySQL Client Libraries and related development files
#
#   GMOCK_FOUND           - TRUE if GMOCK was found
#   GMOCK_INCLUDE_DIRS    - path which contains gmock.h
#   GMOCK_LIBRARIES       - libgmock
#   GMOCK_BOTH_LIBRARIES  - both libgmock and libgmock-main
#   GMOCK_MAIN_LIBRARIES  - libgmock-main
#
# Accept the following variables as input:
#
#   GMOCK_ROOT as CMake variable or environment variable
#     Prefix to installation for GMock.

# Path suffixes when searching for the library, typically "lib"
# TODO Add suffixes for MSVC
set(_libgmock_path_suffixes lib)

function(_gmock_find_library _name)
  find_library(${_name}
    NAMES ${ARGN}
    HINTS ENV GMOCK_ROOT ${GMOCK_ROOT}
    PATH_SUFFIXES ${_libgmock_path_suffixes})
  mark_as_advanced(${_name})
endfunction()

_gmock_find_library(GMOCK_LIBRARY gmock)
_gmock_find_library(GMOCK_MAIN_LIBRARY gmock_main)

find_path(GMOCK_INCLUDE_DIR
  NAMES gmock/gmock.h
  HINTS $ENV{GMOCK_ROOT}/include ${GMOCK_ROOT}/include)
mark_as_advanced(GMOCK_INCLUDE_DIR)

find_package_handle_standard_args(GMock DEFAULT_MSG GMOCK_LIBRARY GMOCK_INCLUDE_DIR GMOCK_MAIN_LIBRARY)

if(GMOCK_FOUND)
  set(GMOCK_INCLUDE_DIRS ${GMOCK_INCLUDE_DIR})
  set(GMOCK_LIBRARIES ${GMOCK_LIBRARY})
  set(GMOCK_MAIN_LIBRARIES ${GMOCK_MAIN_LIBRARY})
  set(GMOCK_BOTH_LIBRARIES ${GMOCK_LIBRARIES} ${GMOCK_MAIN_LIBRARIES})
endif()
