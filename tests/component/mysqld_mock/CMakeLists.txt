# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

################################################################################
# build JSON schema embedder, then use it to generate mysql_server_mock_schema.cc
# (server mock dependency) from mysql_server_mock_schema.js
################################################################################

# build JSON schema embedder
add_executable(json_schema_embedder json_schema_embedder.cc)

# generate mysql_server_mock_schema.cc, which embeds mysql_server_mock_schema.js
# as a c-string. Server mock build will trigger this as its dependency [_HERE_]
add_custom_command(OUTPUT mysql_server_mock_schema.cc
                   COMMAND json_schema_embedder
                     ${CMAKE_CURRENT_SOURCE_DIR}/mysql_server_mock_schema.js
                     ${CMAKE_CURRENT_BINARY_DIR}/mysql_server_mock_schema.cc)





################################################################################
# build server mock
################################################################################

set(mysql_server_mock_sources
  main.cc
  json_statement_reader.cc
  mysql_protocol_decoder.cc
  mysql_protocol_encoder.cc
  mysql_protocol_utils.cc
  mysql_server_mock.cc
  mysql_server_mock_schema.cc # this one is auto-generated [_HERE_]
)

# Implementation files that include RapidJSON headers.
# Used to supress warnings for those.
set(json_sources json_statement_reader.cc)
check_cxx_compiler_flag("-Wshadow" HAVE_SHADOW)
if(HAVE_SHADOW)
  add_compile_flags(${json_sources} COMPILE_FLAGS "-Wno-shadow")
endif()
check_cxx_compiler_flag("-Werror" HAVE_ERROR)
IF(HAVE_ERROR)
  add_compile_flags(${json_sources} COMPILE_FLAGS "-Wno-error")
ENDIF()
check_cxx_compiler_flag("-Wconversion" HAVE_CONVERSION)
IF(HAVE_CONVERSION)
  add_compile_flags(${json_sources} COMPILE_FLAGS "-Wno-conversion")
ENDIF()
check_cxx_compiler_flag("-Wpedantic" HAVE_PEDANTIC)
IF(HAVE_PEDANTIC)
  add_compile_flags(${json_sources} COMPILE_FLAGS "-Wno-pedantic")
ENDIF()


set(common_libraries)
if(WIN32)
  list(APPEND common_libraries Ws2_32.lib)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
  list(APPEND common_libraries -lnsl -lsocket -lpthread)
else()
  list(APPEND common_libraries -lpthread)
endif()

add_executable(mysql_server_mock ${mysql_server_mock_sources})
target_include_directories(mysql_server_mock PUBLIC
  ${RAPIDJSON_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/mysql_protocol/include/)

target_link_libraries(mysql_server_mock ${common_libraries} mysql_protocol)
set_target_output_directory(mysql_server_mock RUNTIME_OUTPUT_DIRECTORY bin)
