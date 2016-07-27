# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

function(ADD_HARNESS_TEST NAME)
  set(_options)
  set(_single_value)
  set(_multi_value SOURCES REQUIRES)
  cmake_parse_arguments(ADD_HARNESS_TEST
    "${_options}" "${_single_value}" "${_multi_value}" ${ARGN})

  add_executable(${NAME} ${ADD_HARNESS_TEST_SOURCES})
  if(ADD_HARNESS_TEST_REQUIRES)
    add_dependencies(${NAME} ${ADD_HARNESS_TEST_REQUIRES})
  endif()
  target_link_libraries(${NAME}
    PRIVATE harness-archive test-helpers ${TEST_LIBRARIES})
  target_compile_definitions(${NAME} PRIVATE -DHARNESS_STATIC_DEFINE)
  add_test(${NAME} ${NAME})
endfunction()


if(WIN32)
  function(CONFIGURE_HARNESS_TEST_FILE SOURCE DESTINATION)
    set(HARNESS_PLUGIN_OUTPUT_DIRECTORY_orig ${HARNESS_PLUGIN_OUTPUT_DIRECTORY})
    foreach(config_ ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${config_} config__)
      set(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${HARNESS_PLUGIN_OUTPUT_DIRECTORY_${config__}})
      configure_file(${SOURCE} ${config_}/${DESTINATION})
    endforeach()
    set(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${HARNESS_PLUGIN_OUTPUT_DIRECTORY_orig})
  endfunction()

  function(CREATE_HARNESS_TEST_DIRECTORY_POST_BUILD TARGET DIRECTORY_NAME)
    foreach(config_ ${CMAKE_CONFIGURATION_TYPES})
      add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${config_}/var/log/${DIRECTORY_NAME})
    endforeach()
  endfunction()
else()
  function(CONFIGURE_HARNESS_TEST_FILE SOURCE DESTINATION)
    configure_file(${SOURCE} ${DESTINATION})
  endfunction()

  function(CREATE_HARNESS_TEST_DIRECTORY_POST_BUILD TARGET DIRECTORY_NAME)
    add_custom_command(TARGET ${TARGET} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/log/${DIRECTORY_NAME})
  endfunction()
endif()
