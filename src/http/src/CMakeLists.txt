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

ADD_LIBRARY(http_server_component
  SHARED
  http_server_component.cc)
TARGET_LINK_LIBRARIES(http_server_component
  ${LIBEVENT_EXTRA}
  ${LIBEVENT_CORE}
  http_common)

ADD_HARNESS_PLUGIN(http_server
  SOURCES http_server_plugin.cc
  static_files.cc
  REQUIRES router_lib;http_server_component)

ADD_LIBRARY(http_client
  SHARED
  http_client.cc
  )

ADD_EXECUTABLE(rest_cli
  rest_cli.cc)
TARGET_LINK_LIBRARIES(rest_cli
  harness-library
  http_client http_common ${LIBEVENT_CORE} ${LIBEVENT_EXTRA})


INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/harness/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/ext/rapidjson/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/router/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/metadata_cache/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/http/include/)
