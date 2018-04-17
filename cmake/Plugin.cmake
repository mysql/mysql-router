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

# add_harness_plugin - Add a new plugin target and set install
#                      location
#
# add_harness_plugin(name [NO_INSTALL]
#                    LOG_DOMAIN domain
#                    SOURCES file1 ...
#                    INTERFACE directory
#                    DESTINATION_SUFFIX string
#                    REQUIRES plugin ...)
#
# The add_harness_plugin command will set up a new plugin target and
# also set the install location of the target correctly.
#
# Plugins that are normally put under the "lib" directory of the build
# root, but see the caveat in the next paragraph.
#
# If NO_INSTALL is provided, it will not be installed, which is useful
# if the plugin is only for testing purposes. These plugins are also
# left in their default location and not moved to the "lib"
# directory. If you want to move the plugin to some specific
# directory, you have to set the target property
# LIBRARY_OUTPUT_DIRECTORY yourself.
#
# If LOG_DOMAIN is given, it will be used as the log domain for the
# plugin. If no LOG_DOMAIN is given, the log domain will be the name
# of the plugin.
#
# If DESTINATION_SUFFIX is provided, it will be appended to the
# destination for install commands. DESTINATION_SUFFIX is optional and
# default to ${HARNESS_NAME}.
#
# Files provided after the SOURCES keyword are the sources to build
# the plugin from, while the files in the directory after INTERFACE
# will be installed alongside the header files for the harness.
#
# For plugins, it is necessary to set the RPATH so that the plugin can
# find other plugins when being loaded. This, unfortunately, means
# that the plugin path need to be set at compile time and cannot be
# changed after that.

function(add_harness_plugin NAME)
  set(_options NO_INSTALL)
  set(_single_value LOG_DOMAIN INTERFACE DESTINATION_SUFFIX OUTPUT_NAME)
  set(_multi_value SOURCES REQUIRES)
  cmake_parse_arguments(_option
    "${_options}" "${_single_value}" "${_multi_value}" ${ARGN})

  if(_option_UNPARSED_ARGUMENTS)
    message(AUTHOR_WARNING
      "Unrecognized arguments: ${_option_UNPARSED_ARGUMENTS}")
  endif()

  # Set default values
  if(NOT _option_DESTINATION_SUFFIX)
    set(_option_DESTINATION_SUFFIX ${HARNESS_NAME})
  endif()

  # Set the log domain to the name of the plugin unless an explicit
  # log domain was given.
  if(NOT _option_LOG_DOMAIN)
    set(_option_LOG_DOMAIN "\"${NAME}\"")
  endif()

  # Add the library and ensure that the name is good for the plugin
  # system (no "lib" before). We are using SHARED libraries since we
  # intend to link against it, which is something that MODULE does not
  # allow. On OSX, this means that the suffix for the library becomes
  # .dylib, which we do not want, so we reset it here.
  add_library(${NAME} SHARED ${_option_SOURCES})
  if(_option_OUTPUT_NAME)
    set_target_properties(${NAME}
      PROPERTIES OUTPUT_NAME ${_option_OUTPUT_NAME})
  endif()
  target_compile_definitions(${NAME} PRIVATE
    MYSQL_ROUTER_LOG_DOMAIN=${_option_LOG_DOMAIN})
  if(NOT WIN32)
    set_target_properties(${NAME} PROPERTIES
      PREFIX ""
      SUFFIX ".so")
  endif()

  # Declare the interface directory for this plugin, if present. It
  # will be used both when compiling the plugin as well as as for any
  # dependent targets.
  if(_option_INTERFACE)
    target_include_directories(${NAME}
      PUBLIC ${_option_INTERFACE})
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${_option_INTERFACE} ${MySQLRouter_BINARY_DIR}/${INSTALL_INCLUDE_DIR})
  endif()

  # Add a dependencies on interfaces for other plugins this plugin
  # requires.
  target_link_libraries(${NAME}
    PUBLIC harness-library
    ${_option_REQUIRES})
  # Need to be able to link plugins with each other
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(${NAME} PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup")
  endif()

  # set library output (and runtime) directories
  if(NOT CMAKE_CFG_INTDIR STREQUAL ".")
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${config} config)
      set_target_properties(${NAME} PROPERTIES
        # [SEARCH TAGS] RUNTIME_OUTPUT_DIRECTORY, LIBRARY_OUTPUT_DIRECTORY
        RUNTIME_OUTPUT_DIRECTORY_${config} ${HARNESS_PLUGIN_OUTPUT_DIRECTORY_${config}}
        LIBRARY_OUTPUT_DIRECTORY_${config} ${HARNESS_PLUGIN_OUTPUT_DIRECTORY_${config}})
    endforeach()
  else()
    set_target_properties(${NAME} PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${HARNESS_PLUGIN_OUTPUT_DIRECTORY}
      RUNTIME_OUTPUT_DIRECTORY ${HARNESS_PLUGIN_OUTPUT_DIRECTORY}
      )
  endif()

  # Add install rules to install the interface header files and the
  # plugin correctly.
  if(NOT _option_NO_INSTALL AND HARNESS_INSTALL_PLUGINS)
    if(WIN32)
      install(TARGETS ${NAME}
        RUNTIME DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR})
      install(FILES $<TARGET_PDB_FILE:${NAME}>
            DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR})
    else()
      install(TARGETS ${NAME}
        LIBRARY DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR}/${_option_DESTINATION_SUFFIX})
    endif()
    if(_option_INTERFACE)
      file(GLOB interface_files ${_option_INTERFACE}/*.h)
      install(FILES ${interface_files}
        DESTINATION ${HARNESS_INSTALL_INCLUDE_PREFIX}/${_option_DESTINATION_SUFFIX})
    endif()
  endif()
endfunction(add_harness_plugin)
