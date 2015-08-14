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

# add_plugin - Add a new plugin target and set install location
#
# add_plugin(name [NO_INSTALL]
#            SOURCES file1 ...
#            INTERFACE directory
#            REQUIRES plugin1 ...)
#
# The add_plugin command will set up a new plugin target and also set
# the install location of the target correctly.
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
# Files provided after the SOURCES keyword are the sources to build
# the plugin from, while the files in the directory after INTERFACE
# will be installed alongside the header files for the harness.
#
# The macro will create two targets:
#
#    <NAME>-INTERFACE is the target for the interface header files.
#    <NAME> is the target for the library.
#
# All plugins are automatically dependent on the harness interface.

macro(ADD_PLUGIN NAME)
  set(sources)
  set(requires "harness-INTERFACE")
  set(NO_INSTALL FALSE)
  set(doing)
  foreach(arg ${ARGN})
    if(arg MATCHES "^NO_INSTALL$")
      set(NO_INSTALL TRUE)
    elseif(arg MATCHES "^(SOURCES|INTERFACE|REQUIRES)$")
      set(doing ${arg})
    elseif(doing MATCHES "^SOURCES$")
      list(APPEND sources ${arg})
    elseif(doing MATCHES "^REQUIRES")
      list(APPEND requires "${arg}-INTERFACE")
    elseif(doing MATCHES "^INTERFACE$")
      set(interface ${arg})
      set(doing)
    else()
      message(AUTHOR_WARNING "Unknown argument: '${arg}'")
    endif()
  endforeach()

  # Add a custom target for the interface which copies it to the
  # staging directory. This is used when building
  if(interface)
    include_directories(${interface})
    add_custom_target("${NAME}-INTERFACE"
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${interface} ${CMAKE_BINARY_DIR}/${INSTALL_INCLUDE_DIR}
      COMMENT "Copying interface from ${CMAKE_CURRENT_SOURCE_DIR}/${interface} to ${CMAKE_BINARY_DIR}/${INSTALL_INCLUDE_DIR}")
    file(GLOB interface_files ${interface}/*.h)
  endif()

  # Add the library and ensure that the name is good for the plugin
  # system (no "lib" before).
  add_library(${NAME} MODULE ${sources})
  set_target_properties(${NAME} PROPERTIES PREFIX "")

  # If the plugin is intended to be installed, we move it to the "lib"
  # directory under the build root. For plugins that are not going to
  # be installed, user have to define the output directory themselves.
  if (NOT NO_INSTALL)
    set_target_properties(${NAME} PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  endif()

  target_link_libraries(${NAME} ${Boost_LIBRARIES})

  # Need to be able to link plugins with each other
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(${NAME} PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup")
  endif()

  # Add a dependencies on interfaces for other plugins this plugin
  # requires.
  if(requires)
    add_dependencies(${NAME} ${requires})
  endif()

  # Add install rules to install the interface and the plugin
  # correctly.
  if(NOT NO_INSTALL AND HARNESS_INSTALL_PLUGINS)
    install(TARGETS ${NAME} LIBRARY DESTINATION lib/${HARNESS_NAME})
    install(FILES ${interface_files} DESTINATION ${INSTALL_INCLUDE_DIR})
  endif()

endmacro(ADD_PLUGIN)

include_directories(${CMAKE_BINARY_DIR}/include)

