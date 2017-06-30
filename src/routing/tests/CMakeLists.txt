# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

include_directories(
  ../include
  ../src
  ${CMAKE_SOURCE_DIR}/src/metadata_cache/include
  ${CMAKE_SOURCE_DIR}/src/mysql_protocol/include
  ${CMAKE_SOURCE_DIR}/src/x_protocol/include
  ../../../tests/helpers
  ${CMAKE_BINARY_DIR}/generated/protobuf
  ${PROTOBUF_INCLUDE_DIR}
)

check_cxx_compiler_flag("-Wshadow" CXX_HAVE_SHADOW)
if(CXX_HAVE_SHADOW)
  add_compile_flags(${ROUTING_SOURCE_FILES} COMPILE_FLAGS
    "-Wno-shadow")
  add_compile_flags(${CMAKE_CURRENT_SOURCE_DIR}/test_x_protocol.cc
    COMPILE_FLAGS "-Wno-shadow")
endif()
check_cxx_compiler_flag("-Wsign-conversion" CXX_HAVE_SIGN_CONVERSION)
if(CXX_HAVE_SIGN_CONVERSION)
  add_compile_flags(${ROUTING_SOURCE_FILES} COMPILE_FLAGS
    "-Wno-sign-conversion")
  add_compile_flags(${CMAKE_CURRENT_SOURCE_DIR}/test_x_protocol.cc
    COMPILE_FLAGS "-Wno-sign-conversion")
endif()
check_cxx_compiler_flag("-Wunused-parameter" CXX_HAVE_UNUSED_PARAMETER)
if(CXX_HAVE_UNUSED_PARAMETER)
  add_compile_flags(${ROUTING_SOURCE_FILES} COMPILE_FLAGS
    "-Wno-unused-parameter")
  add_compile_flags(${CMAKE_CURRENT_SOURCE_DIR}/test_x_protocol.cc
    COMPILE_FLAGS "-Wno-unused-parameter")
endif()
check_cxx_compiler_flag("-Wdeprecated-declarations" CXX_HAVE_DEPRECATED_DECLARATIONS)
if(CXX_HAVE_DEPRECATED_DECLARATIONS)
  add_compile_flags(${ROUTING_SOURCE_FILES} COMPILE_FLAGS
    "-Wno-deprecated-declarations")
  add_compile_flags(${CMAKE_CURRENT_SOURCE_DIR}/test_x_protocol.cc
    COMPILE_FLAGS "-Wno-deprecated-declarations")
endif()

if(MSVC)
  add_compile_flags(${ROUTING_SOURCE_FILES} COMPILE_FLAGS "/DX_PROTOCOL_DEFINE_DYNAMIC"
                                                          "/FImysqlrouter/xprotocol.h")
  add_compile_flags(${CMAKE_CURRENT_SOURCE_DIR}/test_x_protocol.cc COMPILE_FLAGS
                                           "/DX_PROTOCOL_DEFINE_DYNAMIC"
                                           "/FImysqlrouter/xprotocol.h")
else()
  add_compile_flags(${ROUTING_SOURCE_FILES} COMPILE_FLAGS
                                           "-include mysqlrouter/xprotocol.h")
  add_compile_flags(${CMAKE_CURRENT_SOURCE_DIR}/test_x_protocol.cc COMPILE_FLAGS
                                           "-include mysqlrouter/xprotocol.h")
endif(MSVC)

link_directories(${CMAKE_BINARY_DIR}/ext/protobuf/protobuf-3.0.0/cmake/)

add_library(routing_tests STATIC ${ROUTING_SOURCE_FILES})
target_link_libraries(routing_tests routertest_helpers router_lib metadata_cache
                      mysql_protocol x_protocol ${PB_LIBRARY})
set_target_properties(routing_tests PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${STAGE_DIR}/lib)
target_include_directories(routing PRIVATE ${include_dirs})

add_library(routing_plugin_tests STATIC ${ROUTING_PLUGIN_SOURCE_FILES})
target_link_libraries(routing_plugin_tests routing_tests routertest_helpers router_lib metadata_cache)
set_target_properties(routing_plugin_tests PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${STAGE_DIR}/lib)
target_include_directories(routing_plugin_tests PRIVATE ${include_dirs})

target_compile_definitions(routing_plugin_tests PRIVATE -Drouting_DEFINE_STATIC=1)

if(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
  target_link_libraries(routing_tests -lnsl -lsocket)
  target_link_libraries(routing_plugin_tests -lnsl -lsocket)
endif()


add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}
  MODULE "routing"
  LIB_DEPENDS routing_tests test-helpers
  ENVIRONMENT "MYSQL_ROUTER_HOME=${STAGE_DIR}/etc/"
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/mysql_harness/shared/include)

add_test_dir(${CMAKE_CURRENT_SOURCE_DIR}/plugin
  MODULE "routing"
  LIB_DEPENDS
    routing_tests
    routing_plugin_tests
    test-helpers
  ENVIRONMENT "MYSQL_ROUTER_HOME=${STAGE_DIR}/etc/"
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/tests/helpers
    ${CMAKE_SOURCE_DIR}/mysql_harness/shared/include
  )

add_test_dir(issues MODULE issues
  LIB_DEPENDS
    routing_tests
    routing_plugin_tests
    test-helpers
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/tests/helpers
    ${CMAKE_SOURCE_DIR}/mysql_harness/shared/include
  )

set(RUNNING_MYSQL_SERVER "127.0.0.1:3306")
if(WIN32)
  foreach(conf ${CMAKE_CONFIGURATION_TYPES})
    set(ROUTER_STAGE_DIR ${CMAKE_BINARY_DIR}/stage/${conf})
    set(ROUTER_PLUGIN_DIR_SUFFIX "lib")
    configure_file(data/1route.conf.in ${STAGE_DIR}/${conf}/etc/routing_1route.conf)
  endforeach()
else()
  set(ROUTER_STAGE_DIR ${CMAKE_BINARY_DIR}/stage)
  set(ROUTER_PLUGIN_DIR_SUFFIX "lib/mysqlrouter")
  configure_file(data/1route.conf.in ${STAGE_DIR}/etc/routing_1route.conf)
endif()

target_link_libraries(test_issues_bug21962350)
