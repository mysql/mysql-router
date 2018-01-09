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

# Figure out a nice name for Platform and Architecture
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  string(SUBSTRING ${CMAKE_SYSTEM} 7 2 DARWIN_VERSION)
  if(DARWIN_VERSION STREQUAL "17")
    set(PLATFORM_NAME "OS X v10.13")
  elseif(DARWIN_VERSION STREQUAL "16")
    set(PLATFORM_NAME "OS X v10.12")
  elseif(DARWIN_VERSION STREQUAL "15")
    set(PLATFORM_NAME "OS X v10.11")
  elseif(DARWIN_VERSION STREQUAL "14")
    set(PLATFORM_NAME "OS X v10.10")
  elseif(DARWIN_VERSION STREQUAL "13")
    set(PLATFORM_NAME "OS X v10.9")
  else()
    message(FATAL_ERROR "Unsupported version of MacOS X")
  endif()
  set(RPATH_ORIGIN "@loader_path")
  set(CMAKE_INSTALL_NAME_DIR "@rpath")
elseif(CMAKE_SYSTEM_NAME STREQUAL "CYGWIN")
  set(RPATH_ORIGIN "\$ORIGIN")
  set(PLATFORM_NAME "Windows/Cygwin")
else()
  set(RPATH_ORIGIN "\$ORIGIN")
  set(PLATFORM_NAME ${CMAKE_SYSTEM_NAME})
endif()

# Whether we deal with 32 or 64 CPU architecture/compiler
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH_64BIT 1)
else()
  set(ARCH_64BIT 0)
endif()
