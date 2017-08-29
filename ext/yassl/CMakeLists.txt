# Copyright (c) 2006, 2016, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

INCLUDE(${PROJECT_SOURCE_DIR}/cmake/libutils.cmake)

INCLUDE_DIRECTORIES(
  ${MySQLRouter_BINARY_DIR}
  ${MySQLRouter_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/taocrypt/include
  ${CMAKE_CURRENT_SOURCE_DIR}/taocrypt/mySTL)

ADD_DEFINITIONS(${SSL_DEFINES})

# Speed up build process excluding unused header files
ADD_DEFINITIONS(-DWIN32_LEAN_AND_MEAN -DNOGDI)

# We want to use std::min/std::max, not the windows.h macros
ADD_DEFINITIONS(-DNOMINMAX)

# rename get_tty_password to avoid collisions with the main binary
ADD_DEFINITIONS(-Dget_tty_password_ext=yassl_mysql_get_tty_password_ext)
ADD_DEFINITIONS(-Dget_tty_password=yassl_mysql_get_tty_password)

INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

INCLUDE (CheckTypeSize)
CHECK_TYPE_SIZE("long"      SIZEOF_LONG)
CHECK_TYPE_SIZE("long long" SIZEOF_LONG_LONG)

enable_language(CXX)
include(CheckCXXCompilerFlag)

ADD_DEFINITIONS(-DSIZEOF_LONG=${SIZEOF_LONG} -DSIZEOF_LONG_LONG=${SIZEOF_LONG_LONG})
IF(WORDS_BIGENDIAN)
  ADD_DEFINITIONS(-DWORDS_BIGENDIAN=${WORDS_BIGENDIAN})
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wno-sign-conversion" HAVE_NO_SIGN_CONVERSION)
IF(HAVE_NO_SIGN_CONVERSION)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-conversion")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wno-unused-const-variable" HAVE_NO_UNUSED_CONST)
IF(HAVE_NO_UNUSED_CONST)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-const-variable")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wunused-parameter" HAVE_UNUSED)
IF(HAVE_UNUSED)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wno-unused-but-set-variable"
                           HAVE_UNUSED_BUT_SET_VARIABLE)
IF(HAVE_UNUSED_BUT_SET_VARIABLE)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wshadow" HAVE_SHADOW)
IF(HAVE_SHADOW)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-shadow")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wconversion" HAVE_CONVERSION)
IF(HAVE_CONVERSION)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Werror" HAVE_ERROR)
IF(HAVE_ERROR)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")
ENDIF()

SET(YASSL_SOURCES  src/buffer.cpp src/cert_wrapper.cpp src/crypto_wrapper.cpp src/handshake.cpp src/lock.cpp
				src/log.cpp src/socket_wrapper.cpp src/ssl.cpp src/timer.cpp src/yassl_error.cpp
				src/yassl_imp.cpp src/yassl_int.cpp)

ADD_CONVENIENCE_LIBRARY(router_yassl ${YASSL_SOURCES})

ADD_SUBDIRECTORY(taocrypt)
