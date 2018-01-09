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

# Figure out architecture
set(MYSQL_HARNESS_ARCH_CPU ${CMAKE_SYSTEM_PROCESSOR})
message(STATUS "MySQL Harness CPU Descriptor is ${MYSQL_HARNESS_ARCH_CPU}")

# Figure out the operating system. We use lowercase.
string(TOLOWER ${CMAKE_SYSTEM_NAME} MYSQL_HARNESS_ARCH_OS)
message(STATUS "MySQL Harness OS Descriptor is ${MYSQL_HARNESS_ARCH_OS}")

# Figure out the compiler version for the calling conventions and name
# mangling scheme. Format is always <name>-<scheme>.
#
# gcc-2       GNU cc version 2.9x
# gcc-3       GNU cc version 3.x (and later)
#
string(TOLOWER ${CMAKE_CXX_COMPILER_ID} _compiler_id)
if(CMAKE_COMPILER_IS_GNUCXX)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.0)
    set(HARNESS_ARCH_COMPILER "${_compiler_id}-2")
  else()
    set(HARNESS_ARCH_COMPILER "${_compiler_id}-3")
  endif()
else()
  string(REPLACE "." ";" _compiler_version ${CMAKE_CXX_COMPILER_VERSION})
  list(GET _compiler_version 0 _compiler_major)
  set(HARNESS_ARCH_COMPILER "${_compiler_id}-${_compiler_major}")
endif()
message(STATUS "MySQL Harness Compiler Descriptor is ${HARNESS_ARCH_COMPILER}")

# Figure out the runtime library used for building.
set(HARNESS_ARCH_RUNTIME "*")
message(STATUS "MySQL Harness Runtime Descriptor is ${HARNESS_ARCH_RUNTIME}")
