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

# MS Visual C++ specifics
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)  # bypass Warning C4996 (getenv)
endif()

include_directories(../include)
include_directories(${CMAKE_BINARY_DIR}/include)

set(lib_source_files router_app.cc arg_handler.cc utils.cc)
set(source_files main.cc ${lib_source_files})

add_executable(${MYSQL_ROUTER_TARGET} ${source_files})
#add_dependencies(${MYSQL_ROUTER_TARGET} harness-INTERFACE)
target_link_libraries(${MYSQL_ROUTER_TARGET} ${CMAKE_DL_LIBS} harness-library)
target_include_directories(${MYSQL_ROUTER_TARGET} PUBLIC ${CMAKE_BINARY_DIR}/include)
set_target_properties(${MYSQL_ROUTER_TARGET} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${STAGE_DIR}/bin)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(${MYSQL_ROUTER_TARGET} PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup")
elseif(CMAKE_COMPILER_IS_GNUCXX)
endif()

# For testing
if(ENABLE_TESTS)
  add_library(router_test_lib SHARED ${lib_source_files})
  #add_dependencies(router_test_lib harness-INTERFACE)
  target_link_libraries(router_test_lib ${CMAKE_DL_LIBS} harness-library)
endif()

install(TARGETS ${MYSQL_ROUTER_TARGET}
  RUNTIME DESTINATION bin)

file(GLOB ROUTER_INTERFACE ../include/*.h)
install(FILES ${ROUTER_INTERFACE} DESTINATION include/mysql/router)
file(COPY ${ROUTER_INTERFACE} DESTINATION ${CMAKE_BINARY_DIR}/include/mysql/router)

file(GLOB config_files ${CMAKE_CURRENT_SOURCE_DIR}/../tests/*.ini.in)
foreach(config_file ${config_files})
  get_filename_component(dest_config_file ${config_file} NAME)
  string(REGEX REPLACE ".in$" "" dest_config_file ${dest_config_file})
  configure_file(${config_file} ${STAGE_DIR}/etc/${dest_config_file})
endforeach()
