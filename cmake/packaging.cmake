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


if(NOT WIN32)
  set(CPACK_PACKAGE_NAME "mysql-router")
else()
  set(CPACK_PACKAGE_NAME "MySQL Router")
endif()


if(NOT GPL)
  MakeNonGPLPackageName(CPACK_PACKAGE_NAME)
endif()
set(CPACK_PACKAGE_VENDOR "Oracle")
set(CPACK_PACKAGE_CONTACT "MySQL Release Engineering <mysql-build@oss.oracle.com>")

set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION_TEXT})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(EXTRA_NAME_SUFFIX "" CACHE STRING "Extra text in package name")

if(WIN32)
  include(CheckTypeSize)
  if(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(CPACK_SYSTEM_NAME "windows-x86-64bit")
  else()
    set(CPACK_SYSTEM_NAME "windows-x86-32bit")
  endif()
  set(CPACK_PACKAGE_FILE_NAME "mysql-router${EXTRA_NAME_SUFFIX}-${CPACK_PACKAGE_VERSION}${PROJECT_PACKAGE_EXTRAS}-${CPACK_SYSTEM_NAME}")
endif()

#
# Source Distribution
#
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/License.txt")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.txt")
set(CPACK_SOURCE_GENERATOR "ZIP;TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}${PROJECT_PACKAGE_EXTRAS}")

# We ignore all files in the root of the repository and then
# exclude from the list which we want to keep.
file(GLOB cpack_source_ignore_files "${PROJECT_SOURCE_DIR}/*")
set(src_dir ${PROJECT_SOURCE_DIR})
set(source_include
  "${src_dir}/cmake"
  "${src_dir}/include"
  "${src_dir}/doc"
  "${src_dir}/ext"
  "${src_dir}/src"
  "${src_dir}/tests"
  "${src_dir}/tools"
  "${src_dir}/packaging"
  "${src_dir}/CMakeLists.txt"
  "${src_dir}/config.h.in"
  "${src_dir}/README.txt"
  "${src_dir}/License.txt")
list(REMOVE_ITEM cpack_source_ignore_files ${source_include})
list(APPEND cpack_source_ignore_files "${src_dir}/harness/.gitignore")

# We need to escape the dots
string(REPLACE "." "\\\\." cpack_source_ignore_files "${cpack_source_ignore_files}")

set(CPACK_SOURCE_IGNORE_FILES "${cpack_source_ignore_files}")

include(CPack)

#
# RPM-based
#
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_subdirectory("${PROJECT_SOURCE_DIR}/packaging/rpm-oel")
endif()

#
# MSI for Windows
#
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_subdirectory("${PROJECT_SOURCE_DIR}/packaging/WiX")
endif()
