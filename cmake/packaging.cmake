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


set(CPACK_PACKAGE_NAME "mysql-router")
if(NOT GPL)
  MakeNonGPLPackageName(CPACK_PACKAGE_NAME)
endif()
set(CPACK_PACKAGE_VENDOR "Oracle")
set(CPACK_PACKAGE_CONTACT "MySQL Release Engineering <mysql-build@oss.oracle.com>")

set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION_TEXT})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

#
# Source Distribution
#
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/License.txt")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.txt")
set(CPACK_SOURCE_GENERATOR "ZIP;TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")

# We ignore all files in the root of the repository and then
# exclude from the list which we want to keep.
file(GLOB cpack_source_ignore_files "${CMAKE_SOURCE_DIR}/*")
set(src_dir ${CMAKE_SOURCE_DIR})
set(source_include
  "${src_dir}/mysql_harness"
  "${src_dir}/cmake"
  "${src_dir}/doc"
  "${src_dir}/src"
  "${src_dir}/tests"
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
  add_subdirectory("${CMAKE_SOURCE_DIR}/packaging/rpm-oel")
endif()
