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

find_package(Threads REQUIRED)

set(harness_source
  src/loader.cc src/utilities.cc src/config_parser.cc src/designator.cc
  src/filesystem-posix.cc
  src/networking/ip_address.cc
  src/networking/ipv4_address.cc
  src/networking/ipv6_address.cc
  src/networking/resolver.cc)
set(common_libraries ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
  
file(GLOB harness_headers include/*.h)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

configure_file(plugin.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/plugin.h
  ESCAPE_QUOTES @ONLY)

add_library(harness-archive STATIC ${harness_source})
target_link_libraries(harness-archive ${common_libraries})
target_include_directories(harness-archive PUBLIC include)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC harness-archive "-pthread")
endif()

add_library(harness-library SHARED ${harness_source})
target_link_libraries(harness-library ${common_libraries})
target_include_directories(harness-library PUBLIC include)

if(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
  target_link_libraries(harness-library -lnsl -lsocket)
  target_link_libraries(harness-archive -lnsl -lsocket)
endif()

# This is the harness containing a basic main function.
if(ENABLE_HARNESS_PROGRAM)
  add_executable(harness-program src/main.cc)
  set_target_properties(harness-program PROPERTIES
    OUTPUT_NAME harness)
  target_link_libraries(harness-program
    ${common_libraries} harness-library)
  install(TARGETS harness-program RUNTIME DESTINATION bin)
endif()

if(NOT WIN32)
  set_target_properties(harness-archive harness-library PROPERTIES
    OUTPUT_NAME "mysqlharness"
    PREFIX "lib"
    SOVERSION 1)
  set_target_properties(harness-library PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/stage/lib)
else()
  set_target_properties(harness-archive PROPERTIES
    OUTPUT_NAME "mysqlharness_a"
    PREFIX "lib"
    SOVERSION 1)

  # Fix install path in stage dir, so that files get to
  # stage/${configuration_type}/{bin,etc,lib} instead of
  # stage/{bin,etc,lib}/${configuration_type}
  foreach(config_ ${CMAKE_CONFIGURATION_TYPES})
    set_target_properties(harness-library PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${config_} ${CMAKE_BINARY_DIR}/stage/${config_}/bin) # <-- [SEARCH TAG] RUNTIME_OUTPUT_DIRECTORY
  endforeach()

  get_property(harness_library_location TARGET harness-library PROPERTY LOCATION)
  foreach(conf ${CMAKE_CONFIGURATION_TYPES})
    add_custom_command(TARGET harness-library
          COMMAND ${CMAKE_COMMAND} -E copy "${harness_library_location}" ${STAGE_DIR}/${conf}/lib/
          COMMENT "Copying harness library from: ${harness_library_location}"
    )
    file(MAKE_DIRECTORY "${STAGE_DIR}/${conf}/lib/")
  endforeach()
endif()

# copy harness headers
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_BINARY_DIR}/${INSTALL_INCLUDE_DIR})

install(FILES ${harness_headers}
  DESTINATION ${HARNESS_INSTALL_INCLUDE_PREFIX}/${HARNESS_NAME})
install(TARGETS harness-archive harness-library
        LIBRARY DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR}
        ARCHIVE DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR})

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
