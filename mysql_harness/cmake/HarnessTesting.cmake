# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

if(NOT CMAKE_CFG_INTDIR STREQUAL ".")
  function(CONFIGURE_HARNESS_TEST_FILE SOURCE DESTINATION)
    set(HARNESS_PLUGIN_OUTPUT_DIRECTORY_orig ${HARNESS_PLUGIN_OUTPUT_DIRECTORY})
    set(OUT_DIR ${PROJECT_BINARY_DIR}/tests/harness/${config_})
    foreach(config_ ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${config_} config__)
      set(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${HARNESS_PLUGIN_OUTPUT_DIRECTORY_${config__}})
      configure_file(${SOURCE} ${OUT_DIR}/${config_}/${DESTINATION})
    endforeach()
    set(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${HARNESS_PLUGIN_OUTPUT_DIRECTORY_orig})
  endfunction()

  function(CREATE_HARNESS_TEST_DIRECTORY_POST_BUILD TARGET DIRECTORY_NAME)
    set(OUT_DIR ${PROJECT_BINARY_DIR}/tests/harness/)
    foreach(config_ ${CMAKE_CONFIGURATION_TYPES})
      add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUT_DIR}/${config_}/var/log/${DIRECTORY_NAME})
    endforeach()
  endfunction()
else()
  function(CONFIGURE_HARNESS_TEST_FILE SOURCE DESTINATION)
    set(OUT_DIR "${PROJECT_BINARY_DIR}/tests/harness")
    configure_file(${SOURCE} "${OUT_DIR}/${DESTINATION}")
  endfunction()

  function(CREATE_HARNESS_TEST_DIRECTORY_POST_BUILD TARGET DIRECTORY_NAME)
    set(OUT_DIR "${PROJECT_BINARY_DIR}/tests/harness")
    add_custom_command(TARGET ${TARGET} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}/var/log/${DIRECTORY_NAME}")
  endfunction()
endif()
