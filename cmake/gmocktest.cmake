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

# CMake module handling Google C++ Mocking & Testing Framework dependency

# We do not add "REQUIRED" here so we can handle missing GTest
# showing a better error message
find_package(GMock 1.7.0)
set(GTEST_ROOT "${GMOCK_ROOT}/gtest")
find_package(GTest)

if(NOT GTEST_FOUND OR NOT GMOCK_FOUND)
  message(STATUS "")
  message(STATUS "GMock/GTest were not found in following folder")
  message(STATUS "  ${GMOCK_ROOT}")
  message(STATUS "")
  message(STATUS "Please make sure that GMock was compiled in the above")
  message(STATUS "mentioned folder as follows:")
  message(STATUS "  shell> cd ${GMOCK_ROOT}")
  message(STATUS "  shell> cmake .")
  message(STATUS "  shell> make")
  message(STATUS "Or set the CMake variable GMOCK_ROOT pointing")
  message(STATUS "to the installation of GMock (which also includes GTest).")
  message(STATUS "")
  message(STATUS "GMock can be downloaded using following URL:")
  message(STATUS "   ${GMOCK_DOWNLOAD_URL}")
  message(STATUS "")

  message(FATAL_ERROR "Please fix GMock and GTest installation (see above message)")
endif()
