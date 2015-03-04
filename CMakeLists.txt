cmake_minimum_required (VERSION 2.8.10)
project(harness)

# Basic variables
set(HARNESS_NAME harness)

include(CPack)

# Check for C++ 11 support
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)

if(COMPILER_SUPPORTS_CXX11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
  message(STATUS "You need C++11 support, but ${CMAKE_CXX_COMPILER} does not have that.")
endif()

# Check for basic functions

# Increase warning level for code
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wconversion -pedantic")

# add_plugin - Add a new plugin target and set install location
#
# add_plugin(name [NO_INSTALL] file1 ...)
#
# The add_plugin command will set up a new plugin target and also set
# the install location of the target corretly.
#
# If NO_INSTALL is provided, it will not be installed, which is useful
# if the plugin is only for testing purposes.
macro(ADD_PLUGIN NAME)
  set(sources)
  set(NO_INSTALL FALSE)
  foreach(arg ${ARGN})
    if(arg MATCHES "^(NO_INSTALL)$")
      set(NO_INSTALL TRUE)
    else()
      list(APPEND sources ${arg})
    endif()
  endforeach()

  add_library(${NAME} MODULE ${sources})
  set_target_properties(${NAME} PROPERTIES PREFIX "")
 
  if(NOT NO_INSTALL)
    install(TARGETS ${NAME} LIBRARY DESTINATION lib/${HARNESS_NAME})
  endif()
endmacro(ADD_PLUGIN)

include_directories(include)
file(GLOB include_files include/*)
install(FILES ${include_files} DESTINATION "include/mysql/${HARNESS_NAME}")

enable_testing()
 
add_subdirectory(harness)
add_subdirectory(tests)
add_subdirectory(plugins)

