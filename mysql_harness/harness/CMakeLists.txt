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

find_package(Threads REQUIRED)

set(harness_source
  src/loader.cc src/utilities.cc src/config_parser.cc src/designator.cc
  src/filesystem-posix.cc)
file(GLOB harness_headers include/*.h)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

configure_file(plugin.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/plugin.h
  ESCAPE_QUOTES @ONLY)

add_library(harness-archive STATIC ${harness_source})
target_link_libraries(harness-archive dl ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(harness-archive PUBLIC include)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC harness-archive "-pthread")
endif()

add_library(harness-library SHARED ${harness_source})
target_link_libraries(harness-library dl ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(harness-library PUBLIC include)

# This is the harness containing a basic main function.
if(ENABLE_HARNESS_PROGRAM)
  add_executable(harness-program src/main.cc)
  set_target_properties(harness-program PROPERTIES
    OUTPUT_NAME harness)
  target_link_libraries(harness-program
    dl ${CMAKE_THREAD_LIBS_INIT} harness-library)
  install(TARGETS harness-program RUNTIME DESTINATION bin)
endif()

set_target_properties(harness-archive harness-library PROPERTIES
  OUTPUT_NAME "mysqlharness"
  PREFIX "lib"
  SOVERSION 0)

install(FILES ${harness_headers}
  DESTINATION ${HARNESS_INSTALL_INCLUDE_PREFIX}/${HARNESS_NAME})
install(TARGETS harness-archive harness-library
        LIBRARY DESTINATION ${HARNESS_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${HARNESS_INSTALL_LIBDIR})

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
