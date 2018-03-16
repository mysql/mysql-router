# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

# This cmake file builds protoc, the protobuf compiler.
# We also build libprotobuf.a and libprotobuf-lite.a, the protobuf runtime
# support libraries.

# The files in protobuf-3.0.0/ are unmodified versions of google source files.
# To save some space and time, we have remove directories which are not
# needed by MySQL:
#   protobuf-3.0.0/gmock/
#   protobuf-3.0.0/benchmarks/
#   protobuf-3.0.0/examples/
#   protobuf-3.0.0/objectivec/


SET(PROTOBUF_MSVC_DISABLED_WARNINGS "/wd4018 /wd4005 /wd4244 /wd4267 /wd4065")

enable_language(CXX)
include(CheckCXXCompilerFlag)

# Turn off some warning flags when compiling protobuf
CHECK_CXX_COMPILER_FLAG("-Wno-sign-compare" HAVE_NO_SIGN_COMPARE)
IF(HAVE_NO_SIGN_COMPARE)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wno-unused-local-typedefs" HAVE_NO_UNUSED_TYPEDEFS)
IF(HAVE_NO_UNUSED_TYPEDEFS)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wno-ignored-qualifiers" HAVE_NO_IGNORED_QUALIFIERS)
IF(HAVE_NO_IGNORED_QUALIFIERS)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-ignored-qualifiers")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wno-return-type" HAVE_NO_RETURN_TYPE)
IF(HAVE_NO_RETURN_TYPE)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-return-type")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wno-unused-function" HAVE_NO_UNUSED_FUNCTION)
IF(HAVE_NO_UNUSED_FUNCTION)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wmaybe-uninitialized" HAVE_MAYBE_UNINITIALIZED)
IF(HAVE_MAYBE_UNINITIALIZED)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wunused-but-set-parameter" HAVE_UNUSED_BUT_SET)
IF(HAVE_UNUSED_BUT_SET)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-parameter")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wunused-parameter" HAVE_UNUSED)
IF(HAVE_UNUSED)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wshadow" HAVE_SHADOW)
IF(HAVE_SHADOW)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-shadow")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wconversion" HAVE_CONVERSION)
IF(HAVE_CONVERSION)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wpedantic" HAVE_PEDANTIC)
IF(HAVE_PEDANTIC)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pedantic")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Werror" HAVE_ERROR)
IF(HAVE_ERROR)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wenum-compare" HAVE_EC)
IF(HAVE_EC)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-enum-compare")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wdeprecated-declarations" HAVE_DD)
IF(HAVE_DD)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
ENDIF()

# silence
# runtime error: null pointer passed as argument 1, which is declared to never be null
# /usr/include/string.h:43:28: note: nonnull attribute specified here
CHECK_CXX_COMPILER_FLAG("-fno-sanitize=nonnull-attribute" HAVE_NO_SANITIZE_NONULL_ATTRIBUTE)
IF(HAVE_NO_SANITIZE_NONULL_ATTRIBUTE)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-sanitize=nonnull-attribute")
ENDIF()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
ADD_SUBDIRECTORY(protobuf-3.0.0/cmake)
