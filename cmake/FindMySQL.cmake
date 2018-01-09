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

# Find the MySQL Client Libraries and related development files
#
#   MySQL_FOUND           - TRUE if MySQL was found
#   MySQL_INCLUDE_DIRS    - path which contains mysql.h
#   MySQL_LIBRARIES       - libraries provided by the MySQL installation
#   MySQL_VERSION         - version of the MySQL Client Libraries

# In Windows we are linking against the .dll
if(WIN32)
  set(MySQL_CLIENT_LIBRARY libmysql)
else()
  set(MySQL_CLIENT_LIBRARY mysqlclient)
endif()

if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PROGRAMFILES_VAR "PROGRAMW6432")
  else()
    set(PROGRAMFILES_VAR "PROGRAMFILES")
  endif()
  set(WITH_MYSQL "$ENV{${PROGRAMFILES_VAR}}/MySQL/MySQL Server*/" CACHE PATH "Installation path of MySQL Client Libraries")
  if(MYSQL_BUILD)
    STRING(TOLOWER ${MYSQL_BUILD} MYSQL_BUILD)
    set(MySQL_LIBRARY_PATHS
      ${WITH_MYSQL}/lib
      "$ENV{${PROGRAMFILES_VAR}}/MySQL/MySQL Server*/lib"
    )
  else()
    set(MySQL_LIBRARY_PATHS
      ${WITH_MYSQL}/lib
      ${WITH_MYSQL}/lib/debug
      "$ENV{${PROGRAMFILES_VAR}}/MySQL/MySQL Server*/lib"
    )
  endif()

  set(MySQL_INCLUDE_PATHS
    ${WITH_MYSQL}/include
    "$ENV{${PROGRAMFILES_VAR}}/MySQL/MySQL Server*/include"
  )
else()
  set(WITH_MYSQL "/usr/local/mysql" CACHE PATH "Installation path of MySQL Client Libraries")
  set(MySQL_LIBRARY_PATHS
    ${CMAKE_BINARY_DIR}/../mysql-server/lib
    ${WITH_MYSQL}/lib
    /usr/local/mysql/lib
    /usr/local/lib
    /usr/lib/x86_64-linux-gnu
    /usr/lib/i386-linux-gnu
    /usr/lib64
    /usr/lib
  )
  set(MySQL_INCLUDE_PATHS
    ${CMAKE_BINARY_DIR}/../mysql-server/include
    ${WITH_MYSQL}/include
    /usr/local/mysql/include
    /usr/local/include
    /usr/include
  )
endif()

# test if we are located in the mysql-server-tree

IF(EXISTS "${CMAKE_SOURCE_DIR}/include/mysql.h")
  # bundled build
  SET(MySQL_CLIENT_LIB libmysql)
  SET(MySQL_VERSION_HEADER ${CMAKE_BINARY_DIR}/include/mysql_version.h)
  SET(MySQL_INCLUDES ${CMAKE_SOURCE_DIR}/include)
  LIST(APPEND MySQL_INCLUDES ${CMAKE_SOURCE_DIR}/libbinlogevents/export)
  LIST(APPEND MySQL_INCLUDES ${CMAKE_BINARY_DIR}/include)
ELSE()
  # external mysql-server install
  find_path(MySQL_INCLUDES mysql.h PATHS ${MySQL_INCLUDE_PATHS}
            PATH_SUFFIXES mysql NO_DEFAULT_PATH)
  if(WITH_STATIC AND NOT WIN32)
    find_library(MySQL_CLIENT_LIB NAMES lib${MySQL_CLIENT_LIBRARY}.a
                 PATHS ${MySQL_LIBRARY_PATHS} PATH_SUFFIXES mysql
                 NO_DEFAULT_PATH)
  else()
    find_library(MySQL_CLIENT_LIB NAMES ${MySQL_CLIENT_LIBRARY}
                 PATHS ${MySQL_LIBRARY_PATHS} PATH_SUFFIXES mysql
                 NO_DEFAULT_PATH)
    if(NOT WIN32)
      find_library(LIBDL NAMES dl)
    endif()
  endif()
  SET(MySQL_VERSION_HEADER ${MySQL_INCLUDES}/mysql_version.h)
endif()

if(MySQL_INCLUDES AND MySQL_CLIENT_LIB)
  set(MySQL_FOUND TRUE)
  set(MySQL_INCLUDE_DIRS ${MySQL_INCLUDES})
  if(LIBDL)
    set(MySQL_LIBRARIES ${MySQL_CLIENT_LIB} ${LIBDL})
  else()
    set(MySQL_LIBRARIES ${MySQL_CLIENT_LIB})
  endif()
  file(STRINGS "${MySQL_VERSION_HEADER}"
    version_line
    REGEX "^#define[\t ]+MYSQL_SERVER_VERSION.*"
  )
  string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" version_str ${version_line})
  string(REPLACE "." ";" version_list ${version_str})
  list(GET version_list 0 MySQL_VERSION_MAJOR)
  list(GET version_list 1 MySQL_VERSION_MINOR)
  list(GET version_list 2 MySQL_VERSION_PATCH)
  set(MySQL_VERSION "${MySQL_VERSION_MAJOR}.${MySQL_VERSION_MINOR}.${MySQL_VERSION_PATCH}")

  if(MySQL_FIND_VERSION)
    if(MySQL_FIND_VERSION_EXACT AND (NOT MySQL_VERSION VERSION_EQUAL MySQL_FIND_VERSION))
      message(FATAL_ERROR "Exact MySQL v${MySQL_FIND_VERSION} is required; found v${MySQL_VERSION}")
    elseif(MySQL_VERSION VERSION_LESS MySQL_FIND_VERSION)
      message(FATAL_ERROR "MySQL v${MySQL_FIND_VERSION} or later is required; found v${MySQL_VERSION}")
    endif()
  endif()

else()
  set(MySQL_FOUND FALSE)
endif()

if(MySQL_FOUND)
  message(STATUS "Found MySQL Libraries ${MySQL_VERSION}; using ${MySQL_LIBRARIES}")
else()
  if(MySQL_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find MySQL libraries; used ${MySQL_LIBRARY_PATHS}")
  endif()
endif()

mark_as_advanced(MySQL_LIBRARY MySQL_INCLUDE_DIRS)
