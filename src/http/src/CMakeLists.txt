# Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

ADD_LIBRARY(http_common
  SHARED
  http_common.cc
  http_time.cc
  )
TARGET_LINK_LIBRARIES(http_common
  ${LIBEVENT2_EXTRA}
  ${LIBEVENT2_CORE})

ADD_HARNESS_PLUGIN(http_server
  NO_INSTALL
  SOURCES http_server_plugin.cc
  static_files.cc
  http_server_component.cc
  REQUIRES router_lib;http_common)

## place event.dll into the same dir as http_common
##
## setting PATH isn't good enough as the windows has an event.dll
## in system paths which would take precedence
IF (WIN32 AND (NOT WITH_LIBEVENT STREQUAL "system"))
  SET(LIBEVENT_NAME event)

  ADD_CUSTOM_COMMAND(TARGET http_common PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${WITH_LIBEVENT}/bin/${LIBEVENT_NAME}.dll"
    "$<TARGET_FILE_DIR:http_common>/${LIBEVENT_NAME}.dll"
    )
ENDIF()

ADD_LIBRARY(http_client
  SHARED
  http_client.cc
  rest_client.cc
  )
TARGET_LINK_LIBRARIES(http_client
  ${LIBEVENT2_EXTRA}
  ${LIBEVENT2_CORE}
  http_common)

ADD_EXECUTABLE(rest_cli
  rest_cli.cc)
TARGET_LINK_LIBRARIES(rest_cli
  harness-library
  http_client)


INCLUDE_DIRECTORIES(${LIBEVENT2_INCLUDE_DIR}/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/harness/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/ext/rapidjson/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/router/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/metadata_cache/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/http/include/)
