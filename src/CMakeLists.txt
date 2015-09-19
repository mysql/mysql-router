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

set(CURR_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB MODULE_DIRS
   RELATIVE ${CURR_DIR}
   ${CURR_DIR}/*)

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
