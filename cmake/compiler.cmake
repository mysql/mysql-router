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

include(CheckCXXCompilerFlag)

# Check for C++11 support
function(CHECK_CXX11)
  check_cxx_compiler_flag("-std=c++11" support_11)

  if(support_11)
    set(CXX11_FLAG "-std=c++11" PARENT_SCOPE)
  else()
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER} does not support C++11 standard")
  endif()
  set(CMAKE_CXX_FLAGS ${CXX11_FLAG} PARENT_SCOPE)
endfunction()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  check_cxx11()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -Wconversion -Wshadow")

  check_cxx_compiler_flag("-Wpedantic" COMPILER_HAS_WARNING_PEDANTIC)
  if(COMPILER_HAS_WARNING_PEDANTIC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.9)
    # gcc 4.8 doesn't support -Wpedantic -Wno-pedantic to selectively disable
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=pedantic")
  endif()
  # bring the options in sync with the server's

  # GCC has this options, clang doesn't
  check_cxx_compiler_flag("-Wformat-security" COMPILER_HAS_WARNING_FORMAT_SECURITY)
  if(COMPILER_HAS_WARNING_FORMAT_SECURITY)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat-security")
  endif()

  check_cxx_compiler_flag("-Wnon-virtual-dtor" COMPILER_HAS_WARNING_NON_VIRTUAL_DTOR)
  if(COMPILER_HAS_WARNING_NON_VIRTUAL_DTOR)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnon-virtual-dtor")
  endif()

  # GCC/clang have this option
  # clang doesn't have its sibling: -Wsuggest-attribute=format
  check_cxx_compiler_flag("-Wmissing-format-attribute" COMPILER_HAS_WARNING_MISSING_FORMAT_ATTRIBUTE)
  if(COMPILER_HAS_WARNING_MISSING_FORMAT_ATTRIBUTE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmissing-format-attribute")
  endif()

  # GCC/clang have this option
  #
  # FIXME: as long as protobuf-3.0.0 and gmock 1.7.0 are used, we need to disable -Wundef as it is
  # triggered all over the place in the headers. It should actually be enabled.
  check_cxx_compiler_flag("-Wundef" COMPILER_HAS_WARNING_UNDEF)
  if(COMPILER_HAS_WARNING_UNDEF)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-undef -Wno-conversion")
  endif()

  # GCC/clang have this option
  check_cxx_compiler_flag("-Wvla" COMPILER_HAS_WARNING_VLA)
  if(COMPILER_HAS_WARNING_VLA)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wvla")
  endif()

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX11_FLAG}")

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Overview of MSVC versions: http://www.cmake.org/cmake/help/v3.3/variable/MSVC_VERSION.html
  if("${MSVC_VERSION}" VERSION_LESS 1800)
    message(FATAL_ERROR "Need at least ${CMAKE_CXX_COMPILER} 12.0")
  endif()
  # /TP is needed so .cc files are recognoized as C++ source files by MSVC
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /TP")
  add_definitions(-DWIN32_LEAN_AND_MEAN)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.13.0)
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION} is too old; need at least SunPro 5.13.0 (aka Oracle Developer Studio 12.4)")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
  message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER} is not supported")
endif()

