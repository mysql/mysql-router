# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

# Find the MySQL Client Libraries and related development files
#
#   GMOCK_FOUND           - TRUE if GMock was found
#   GMOCK_INCLUDE_DIRS    - path which contains gmock.h
#   GMOCK_LIBRARIES       - libgmock
#   GMOCK_BOTH_LIBRARIES  - both libgmock and libgmock-main
#   GMOCK_MAIN_LIBRARIES  - libgmock-main

set(GMOCK_FOUND FALSE)

# Can not use IS_ABSOLUTE as paths with ~ (home directory) are not starting with
get_filename_component(GMOCK_ROOT ${GMOCK_ROOT} ABSOLUTE)

find_library(gmock_lib NAMES gmock PATHS ${GMOCK_ROOT})
find_path(gmock_inc_dir NAMES gmock/gmock.h PATHS ${GMOCK_ROOT}/include)
# File containing version
find_path(gmock_configure NAMES configure PATHS ${GMOCK_ROOT})

if(gmock_lib AND gmock_inc_dir AND gmock_configure)

  # Not the greatest place for getting the version, but best we got
  file(STRINGS "${gmock_configure}/configure"
    version_line
    REGEX "^PACKAGE_VERSION='.*'"
  )
  string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" version_str ${version_line})
  string(REPLACE "." ";" version_list ${version_str})
  list(GET version_list 0 GMOCK_VERSION_MAJOR)
  list(GET version_list 1 GMOCK_VERSION_MINOR)
  list(GET version_list 2 GMOCK_VERSION_PATCH)
  set(GMOCK_VERSION "${GMOCK_VERSION_MAJOR}.${GMOCK_VERSION_MINOR}.${GMOCK_VERSION_PATCH}")

  if(GMock_FIND_VERSION)
    if(GMock_FIND_VERSION_EXACT AND (NOT GMOCK_VERSION VERSION_EQUAL GMock_FIND_VERSION))
        message(FATAL_ERROR "Exact GMock v${GMock_FIND_VERSION} is required; found v${GMOCK_VERSION}")
    elseif(GMOCK_VERSION VERSION_LESS GMock_FIND_VERSION)
      message(FATAL_ERROR "GMock v${GMock_FIND_VERSION} or later is required; found v${GMOCK_VERSION}")
    endif()
  endif()

  set(GMOCK_FOUND TRUE)
  set(GMOCK_INCLUDE_DIRS ${gmock_inc_dir})
  set(GMOCK_LIBRARIES ${gmock_lib})
  find_library(GMOCK_MAIN_LIBRARIES NAMES gmock_main HINTS ${ENV_GMOCK_ROOT} ${GMOCK_ROOT})
  set(GMOCK_BOTH_LIBRARIES ${GMOCK_MAIN_LIBRARIES} ${GMOCK_LIBRARIES})
  message(STATUS "Found GMock: ${gmock_lib}")
endif()

if(GMock_FIND_REQUIRED AND NOT GMOCK_FOUND)
  message(FATAL_ERROR "Google C++ Mocking Framework not found under '${GMOCK_ROOT}'")
endif()
