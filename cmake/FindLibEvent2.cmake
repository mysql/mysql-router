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
  IF(NOT WIN32)
    SET(LIBEVENT2_INCLUDE_PATH /usr/local/include /opt/local/include)
    SET(LIBEVENT2_LIB_PATHS /usr/local/lib /opt/local/lib)
  ENDIF()

  # use default paths
  SET(HOW_TO_FIND)
ELSEIF(WITH_LIBEVENT STREQUAL "bundled")
  MESSAGE(FATAL_ERROR "bundled libevent isn't support")
ELSE()
  # make the users path for libevent absolute
  GET_FILENAME_COMPONENT(LIBEVENT_ABS_DIR "${WITH_LIBEVENT}" ABSOLUTE)
  SET(LIBEVENT2_INCLUDE_PATH ${LIBEVENT_ABS_DIR}/include)
  SET(LIBEVENT2_LIB_PATHS ${LIBEVENT_ABS_DIR}/lib)

  # if path specified, use that path only
  SET(HOW_TO_FIND NO_DEFAULT_PATH)
ENDIF()

FIND_PATH(LIBEVENT2_INCLUDE_DIR event2/event.h PATHS ${LIBEVENT2_INCLUDE_PATH} ${HOW_TO_FIND})
IF(WIN32)
  ## libevent-2.0.22 on windows is only 'event.lib' and 'event.dll'
  FIND_LIBRARY(LIBEVENT2_CORE NAMES event PATHS ${LIBEVENT2_LIB_PATHS} ${HOW_TO_FIND})
  SET(LIBEVENT2_EXTRA)
ELSE()
  FIND_LIBRARY(LIBEVENT2_CORE NAMES event_core PATHS ${LIBEVENT2_LIB_PATHS} ${HOW_TO_FIND})
  FIND_LIBRARY(LIBEVENT2_EXTRA NAMES event_extra PATHS ${LIBEVENT2_LIB_PATHS} ${HOW_TO_FIND})
ENDIF()

IF (LIBEVENT2_INCLUDE_DIR AND LIBEVENT2_CORE)
  SET(LibEvent2_FOUND TRUE)
ELSE()
  SET(LibEvent2_FOUND FALSE)
ENDIF()

IF(LibEvent2_FIND_VERSION)
  IF(LibEvent2_FIND_VERSION_EXACT)
    SET(LIBEVENT2_VERSION_REQUEST_STR "requested == ${LibEvent2_FIND_VERSION}")
  ELSE()
    SET(LIBEVENT2_VERSION_REQUEST_STR "requested >= ${LibEvent2_FIND_VERSION}")
  ENDIF()
ELSE()
  SET(LIBEVENT2_VERSION_REQUEST_STR "requested any")
ENDIF()

IF (LibEvent2_FOUND)
  # extract version number from event-config.h
  #
  # libevent-2.0 has _EVENT_VERSION
  # libevent-2.1 has EVENT__VERSION
  FILE(WRITE ${PROJECT_BINARY_DIR}/check-libevent-header-version.c
    "#include <event2/event.h>
    #include <stdio.h>
    int main() {
    puts(LIBEVENT_VERSION);
    return 0;
    };
    ")
  TRY_RUN(LIBEVENT_VERSION_RUN_RES LIBEVENT_VERSION_COMPILE_RES
    ${PROJECT_BINARY_DIR}
    ${PROJECT_BINARY_DIR}/check-libevent-header-version.c
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LIBEVENT2_INCLUDE_DIR}"
    COMPILE_OUTPUT_VARIABLE compile_output
    RUN_OUTPUT_VARIABLE version_line
    )

  IF(NOT version_line)
    ## debug output in case of compile/run failures
    MESSAGE(STATUS "compile-result: ${LIBEVENT_VERSION_COMPILE_RES}")
    MESSAGE(STATUS "compile-output: ${compile_output}")
    MESSAGE(STATUS "run-result: ${LIBEVENT_VERSION_RUN_RES}")
    MESSAGE(STATUS "run-output: ${version_line}")
    MESSAGE(FATAL_ERROR "Could NOT find version-line in ${LIBEVENT2_INCLUDE_DIR}/event2/event-config.h")
  ENDIF()

  STRING(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" version_str ${version_line})
  STRING(REPLACE "." ";" version_list ${version_str})
  LIST(GET version_list 0 LIBEVENT2_VERSION_MAJOR)
  LIST(GET version_list 1 LIBEVENT2_VERSION_MINOR)
  LIST(GET version_list 2 LIBEVENT2_VERSION_PATCH)
  SET(LIBEVENT2_VERSION "${LIBEVENT2_VERSION_MAJOR}.${LIBEVENT2_VERSION_MINOR}.${LIBEVENT2_VERSION_PATCH}")

  IF(LibEvent2_FIND_VERSION)
    IF(LibEvent2_FIND_VERSION_EXACT)
      IF(NOT LIBEVENT2_VERSION VERSION_EQUAL "${LibEvent2_FIND_VERSION}")
        MESSAGE(FATAL_ERROR "Found libevent 2.x: ${LIBEVENT2_CORE}, version=${LIBEVENT2_VERSION}, but ${LIBEVENT2_VERSION_REQUEST_STR}")
      ENDIF()
    ELSE()
      IF(LIBEVENT2_VERSION VERSION_LESS "${LibEvent2_FIND_VERSION}")
        MESSAGE(FATAL_ERROR "Found libevent 2.x: ${LIBEVENT2_CORE}, version=${LIBEVENT2_VERSION}, but ${LIBEVENT2_VERSION_REQUEST_STR}")
      ENDIF()
    ENDIF()
  ENDIF()

  # we want 2.x and higher

  IF(NOT LibEvent2_FIND_QUIETLY)
    MESSAGE(STATUS "Found libevent 2.x: ${LIBEVENT2_CORE}, version=${LIBEVENT2_VERSION} (${LIBEVENT2_VERSION_REQUEST_STR})")
  ENDIF()
ELSE()
  IF(LibEvent2_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could NOT find libevent 2.x libs in ${LIBEVENT2_LIB_PATHS}, headers in ${LIBEVENT2_INCLUDE_PATH}. (${LIBEVENT2_VERSION_REQUEST_STR})")
  ENDIF()
ENDIF()

# don't expose them in the cmake UI
MARK_AS_ADVANCED(
  LIBEVENT2_INCLUDE_DIR
  LIBEVENT2_CORE
  LIBEVENT2_EXTRA
  LIBEVENT2_VERSION
)
