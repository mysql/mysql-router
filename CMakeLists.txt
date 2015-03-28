cmake_minimum_required (VERSION 2.8.10)
project(harness)

set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Basic variables
set(HARNESS_NAME harness)
set(INSTALL_INCLUDE_DIR "include/mysql/${HARNESS_NAME}")

include(CPack)
include(Plugin)

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

enable_testing()
 
add_subdirectory(harness)
add_subdirectory(tests)
add_subdirectory(plugins)

