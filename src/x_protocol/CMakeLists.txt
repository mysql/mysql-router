# Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
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

include_directories(${PROTOBUF_INCLUDE_DIRS}
  "${CMAKE_CURRENT_SOURCE_DIR}/include/mysqlrouter"
)
include(${PROJECT_SOURCE_DIR}/cmake/mysqlx_protobuf.cmake)
include(CheckCXXCompilerFlag)

set(PROTOBUF_MYSQLX_FILES
  "${PROTOBUF_MYSQLX_DIR}/mysqlx.proto"
  "${PROTOBUF_MYSQLX_DIR}/mysqlx_datatypes.proto"
  "${PROTOBUF_MYSQLX_DIR}/mysqlx_connection.proto"
  "${PROTOBUF_MYSQLX_DIR}/mysqlx_expect.proto"
  "${PROTOBUF_MYSQLX_DIR}/mysqlx_expr.proto"
  "${PROTOBUF_MYSQLX_DIR}/mysqlx_crud.proto"
  "${PROTOBUF_MYSQLX_DIR}/mysqlx_sql.proto"
  "${PROTOBUF_MYSQLX_DIR}/mysqlx_session.proto"
  "${PROTOBUF_MYSQLX_DIR}/mysqlx_notice.proto"
  "${PROTOBUF_MYSQLX_DIR}/mysqlx_resultset.proto"
)

mysqlx_protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTOBUF_MYSQLX_FILES})

check_cxx_compiler_flag("-Wignored-qualifiers" HAVE_NO_IGNORED_QUALIFIERS)
if(HAVE_NO_IGNORED_QUALIFIERS)
  add_compile_flags(${PROTO_SRCS} COMPILE_FLAGS
    "-Wno-ignored-qualifiers")
endif()

check_cxx_compiler_flag("-Wsign-compare" CXX_HAVE_SIGN_COMPARE)
if(CXX_HAVE_SIGN_COMPARE)
  add_compile_flags(${PROTO_SRCS} COMPILE_FLAGS
    "-Wno-sign-compare")
ENDIF()

check_cxx_compiler_flag("-Wshadow" CXX_HAVE_SHADOW)
if(CXX_HAVE_SHADOW)
  add_compile_flags(${PROTO_SRCS} COMPILE_FLAGS
    "-Wno-shadow")
endif()

check_cxx_compiler_flag("-Wpedantic" CXX_HAVE_PEDANTIC)
if(CXX_HAVE_PEDANTIC)
  add_compile_flags(${PROTO_SRCS} COMPILE_FLAGS
    "-Wno-pedantic")
endif()

check_cxx_compiler_flag("-Wconversion" CXX_HAVE_CONVERSION)
if(CXX_HAVE_CONVERSION)
  add_compile_flags(${PROTO_SRCS} COMPILE_FLAGS
    "-Wno-conversion")
endif()

check_cxx_compiler_flag("-Wunused-parameter" CXX_HAVE_UNUSED_PARAMETER)
if(CXX_HAVE_UNUSED_PARAMETER)
  add_compile_flags(${PROTO_SRCS} COMPILE_FLAGS
    "-Wno-unused-parameter")
endif()

check_cxx_compiler_flag("-Wdeprecated-declarations" CXX_HAVE_DEPRECATED_DECLARATIONS)
if(CXX_HAVE_DEPRECATED_DECLARATIONS)
  add_compile_flags(${PROTO_SRCS} COMPILE_FLAGS
    "-Wno-deprecated-declarations")
endif()

if(MSVC)
  add_compile_flags(${PROTO_SRCS} COMPILE_FLAGS "/wd4018")
  add_compile_flags(${PROTO_SRCS} COMPILE_FLAGS "/DX_PROTOCOL_DEFINE_DYNAMIC" "/DX_PROTOCOL_EXPORTS"
                                                "/FIxprotocol.h")
else()
  add_compile_flags(${PROTO_SRCS} COMPILE_FLAGS "-include xprotocol.h")
endif(MSVC)

set(SOURCE_FILES ${PROTO_SRCS} ${PROTO_HDRS})

set(include_dirs
  ../src/router/include
  #include
  "${PROJECT_BINARY_DIR}/generated/protobuf"
  )

# link_directories(${PROJECT_BINARY_DIR}/ext/protobuf/protobuf-3.0.0/cmake/)
add_harness_plugin(x_protocol
  SOURCES ${SOURCE_FILES}
  REQUIRES router_lib)
target_include_directories(x_protocol PRIVATE ${include_dirs})

target_link_libraries(x_protocol PUBLIC ${PROTOBUF_LIBRARY})


# don't install headers until a) a final destination is found and b) API is stable
# file(GLOB xprotocol_headers "${PROJECT_BINARY_DIR}/generated/protobuf/*.h")
# install(FILES ${xprotocol_headers}
#   DESTINATION "include/${HARNESS_NAME}/x_protocol")

#if(ENABLE_TESTS)
  #add_subdirectory(tests/)
#endif()
