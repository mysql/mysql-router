# Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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

include_directories(${PROTOBUF_INCLUDE_DIRS}
  "${CMAKE_CURRENT_SOURCE_DIR}/include/mysqlrouter"
)
include(${CMAKE_SOURCE_DIR}/cmake/mysqlx_protobuf.cmake)
include(CheckCXXCompilerFlag)

set(PROTOBUF_MYSQLX_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/proto/mysqlx.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/proto/mysqlx_datatypes.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/proto/mysqlx_connection.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/proto/mysqlx_expect.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/proto/mysqlx_expr.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/proto/mysqlx_crud.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/proto/mysqlx_sql.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/proto/mysqlx_session.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/proto/mysqlx_notice.proto"
  "${CMAKE_CURRENT_SOURCE_DIR}/proto/mysqlx_resultset.proto"
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
  "${CMAKE_BINARY_DIR}/generated/protobuf"
  )

link_directories(${CMAKE_BINARY_DIR}/ext/protobuf/protobuf-3.0.0/cmake/)
add_harness_plugin(x_protocol
  SOURCES ${SOURCE_FILES}
  REQUIRES router_lib logger)
target_include_directories(x_protocol PRIVATE ${include_dirs})

target_link_libraries(x_protocol PUBLIC optimized ${PROTOBUF_LIBRARY} debug ${PROTOBUF_LIBRARY_DEBUG})


file(GLOB xprotocol_headers "${CMAKE_BINARY_DIR}/generated/protobuf/*.h")
install(FILES ${xprotocol_headers}
  DESTINATION "include/${HARNESS_NAME}/x_protocol")

#if(ENABLE_TESTS)
  #add_subdirectory(tests/)
#endif()
