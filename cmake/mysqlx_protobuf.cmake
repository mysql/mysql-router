# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

IF(MSVC)
  SET(MYSQLX_PROTOBUF_MSVC_DISABLED_WARNINGS "/wd4267 /wd4244")
ENDIF()

# Standard PROTOBUF_GENERATE_CPP modified to generate both
# protobuf and protobuf-lite C++ files.
FUNCTION(MYSQLX_PROTOBUF_GENERATE_CPP SRCS HDRS)
  IF(NOT ARGN)
    MESSAGE(SEND_ERROR
      "Error: MYSQLX_PROTOBUF_GENERATE_CPP() called without any proto files")
    RETURN()
  ENDIF()

  SET(${SRCS})
  SET(${HDRS})
  FOREACH(FIL ${ARGN})
    GET_FILENAME_COMPONENT(ABS_FIL ${FIL} ABSOLUTE)
    GET_FILENAME_COMPONENT(FIL_WE ${FIL} NAME_WE)

    LIST(APPEND ${SRCS} "${PROJECT_BINARY_DIR}/generated/protobuf/${FIL_WE}.pb.cc")
    LIST(APPEND ${HDRS} "${PROJECT_BINARY_DIR}/generated/protobuf/${FIL_WE}.pb.h")

    ADD_CUSTOM_COMMAND(
      OUTPUT "${PROJECT_BINARY_DIR}/generated/protobuf/${FIL_WE}.pb.cc"
             "${PROJECT_BINARY_DIR}/generated/protobuf/${FIL_WE}.pb.h"
      COMMAND ${CMAKE_COMMAND}
              -E make_directory "${PROJECT_BINARY_DIR}/generated/protobuf"
      COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS --cpp_out=dllexport_decl=X_PROTOCOL_API:${PROJECT_BINARY_DIR}/generated/protobuf
              -I "${PROTOBUF_MYSQLX_DIR}" -I "${PROTOBUF_INCLUDE_DIR}" ${ABS_FIL}
      DEPENDS ${ABS_FIL} ${PROTOBUF_PROTOC_EXECUTABLE}
      COMMENT "Running C++ protocol buffer compiler on ${FIL}"
      VERBATIM)
  ENDFOREACH()

  SET_SOURCE_FILES_PROPERTIES(
    ${${SRCS}} ${${HDRS}}
    PROPERTIES GENERATED TRUE)

  IF(MSVC)
    ADD_COMPILE_FLAGS(${${SRCS}}
      COMPILE_FLAGS ${MYSQLX_PROTOBUF_MSVC_DISABLED_WARNINGS})
  ENDIF()

  SET(${SRCS} ${${SRCS}} PARENT_SCOPE)
  SET(${HDRS} ${${HDRS}} PARENT_SCOPE)
ENDFUNCTION()

