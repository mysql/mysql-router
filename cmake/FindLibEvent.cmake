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

IF(NOT WITH_LIBEVENT)
  SET(WITH_LIBEVENT system)
ENDIF()

IF(WITH_LIBEVENT STREQUAL "system" OR WITH_LIBEVENT STREQUAL "yes")
  SET(LIBEVENT_INCLUDE_PATH ${WITH_LIBEVENT}/include)
  SET(LIBEVENT_LIB_PATHS ${WITH_LIBEVENT}/lib)
ELSEIF(WITH_LIBEVENT STREQUAL "bundled")
  MESSAGE(FATAL_ERROR "bundled libevent isn't support")
ELSE()
  SET(LIBEVENT_INCLUDE_PATH /usr/local/include /opt/local/include)
  SET(LIBEVENT_LIB_PATHS /usr/local/lib /opt/local/lib)
ENDIF()

FIND_PATH(LIBEVENT_INCLUDE_DIR event.h PATHS ${LIBEVENT_INCLUDE_PATH})
FIND_LIBRARY(LIBEVENT_LIB NAMES event PATHS ${LIBEVENT_LIB_PATHS})
FIND_LIBRARY(LIBEVENT_CORE NAMES event_core PATHS ${LIBEVENT_LIB_PATHS})
FIND_LIBRARY(LIBEVENT_EXTRA NAMES event_extra PATHS ${LIBEVENT_LIB_PATHS})

IF (LIBEVENT_INCLUDE_DIR AND LIBEVENT_CORE)
  SET(LibEvent_FOUND TRUE)
ELSE()
  SET(LibEvent_FOUND FALSE)
ENDIF()

IF (LibEvent_FOUND)
  IF(NOT LibEvent_FIND_QUIETLY)
    MESSAGE(STATUS "Found libevent: ${LIBEVENT_LIB}")
  ENDIF()
ELSE()
  IF(LibEvent_REQUIRED)
    MESSAGE(FATAL_ERROR "Could NOT find libevent")
  ENDIF()
ENDIF()

MARK_AS_ADVANCED(
  LIBEVENT_INCLUDE_DIR
  LIBEVENT_LIB
  LIBEVENT_CORE
  LIBEVENT_EXTRA
)
