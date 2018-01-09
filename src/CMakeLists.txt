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

set(CURR_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB MODULE_DIRS
   RELATIVE ${CURR_DIR}
   ${CURR_DIR}/*)
   
list(SORT MODULE_DIRS)

# Load all modules
foreach(module_name ${MODULE_DIRS})
  set(mod_dir "${CURR_DIR}/${module_name}")
  if(IS_DIRECTORY ${mod_dir})
    if(EXISTS "${mod_dir}/CMakeLists.txt")
      message(STATUS "Loading module '${module_name}'")
      add_subdirectory(${module_name})
    else()
      message(STATUS "Module in folder '${module_name}' has no CMakeLists.txt and/or include-folder")
    endif()
  endif()
  unset(mod_dir)
endforeach()
