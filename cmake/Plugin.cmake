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
# If NO_INSTALL is provided, it will not be installed, which is useful
# if the plugin is only for testing purposes.
#
# Files provided after the SOURCES keyword are the sources to build
# the plugin from, while the files in the directory after INTERFACE
# will be installed alongside the header files for the harness.
#
# The macro will create two targets:
#
#    <NAME>-INTERFACE is the target for the interface.
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
  endif()

  # Add the library and ensure that the name is good for the plugin
  # system (no "lib" before).
  add_library(${NAME} MODULE ${sources})
  set_target_properties(${NAME} PROPERTIES PREFIX "")

  # Add a dependencies on interfaces for other plugins this plugin
  # requires.
  if(requires)
    add_dependencies(${NAME} ${requires})
  endif()

  # Add install rules to install the interface and the plugin
  # correctly.
  if(NOT NO_INSTALL)
    install(TARGETS ${NAME} LIBRARY DESTINATION lib/${HARNESS_NAME})
    install(FILES ${headers} DESTINATION ${INSTALL_INCLUDE_DIR})
  endif()

endmacro(ADD_PLUGIN)

include_directories(${CMAKE_BINARY_DIR}/include)

