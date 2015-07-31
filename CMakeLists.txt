cmake_minimum_required (VERSION 2.8.10)

if(${CMAKE_VERSION} VERSION_GREATER "3.0")
    cmake_policy(SET CMP0042 NEW) # MACOSX_RPATH is enabled by default
    cmake_policy(SET CMP0048 NEW)
endif()

project(harness)

set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Basic variables
set(HARNESS_NAME "harness" CACHE STRING "Name of Harness")
set(INSTALL_INCLUDE_DIR "include/mysql/harness")
set(HARNESS_INSTALL_PROGRAM YES CACHE BOOL "Whether to install the Harness main program")
set(HARNESS_INSTALL_PLUGINS YES CACHE BOOL "Whether to install the Harness provided plugins")

include(CPack)
include(Plugin)
include(Docs)

# Check for C++ 11 support
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)

if(COMPILER_SUPPORTS_CXX11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
  message(FATAL_ERROR "You need C++11 support, but ${CMAKE_CXX_COMPILER} does not have that.")
endif()

# Check for basic functions
find_package(Threads REQUIRED)

# Increase warning level for code
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -Wconversion -Wpedantic -Wshadow")

enable_testing()
add_subdirectory(harness)
add_subdirectory(tests)
add_subdirectory(plugins)

