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

# Version information of MySQL Router

# Project version, has to be an X.Y.Z number since it is used with the
# "project" CMake command
set(PROJECT_VERSION_TEXT "8.0.12")

# Can be arbitrary test that is added to the package file names after
# the version, but before the extensions.
set(PROJECT_PACKAGE_EXTRAS "")

set(PROJECT_EDITION "GPL community edition" CACHE STRING
  "Edition of MySQL Router")

# create a string that is allowed in a RPM spec "release" field
set(RPM_EXTRA_VERSION "${PROJECT_PACKAGE_EXTRAS}")
if(RPM_EXTRA_VERSION)
  string(REGEX REPLACE "[^A-Za-z0-9]" "" RPM_EXTRA_VERSION "${RPM_EXTRA_VERSION}")
  set(RPM_EXTRA_VERSION ".${RPM_EXTRA_VERSION}")
endif()

# Nothing below this line needs change when releasing

# Older CMake version do not set PROJECT_VERSION
if(${CMAKE_VERSION} VERSION_LESS "3.0")
  # We can not use project() to set version information
  string(REPLACE "." ";" version_list ${PROJECT_VERSION_TEXT})
  list(GET version_list 0 major)
  list(GET version_list 1 minor)
  list(GET version_list 2 patch)
  set(MySQLRouter_VERSION_MAJOR ${major})
  set(MySQLRouter_VERSION_MINOR ${minor})
  set(MySQLRouter_VERSION_PATCH ${patch})
  set(PROJECT_VERSION_MAJOR ${major})
  set(PROJECT_VERSION_MINOR ${minor})
  set(PROJECT_VERSION_PATCH ${patch})
  set(MySQLRouter_VERSION ${PROJECT_VERSION_TEXT})
  set(PROJECT_VERSION ${PROJECT_VERSION_TEXT})
endif()
