# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

# CMake module handling Google C++ Testing Framework dependency

# We do not add "REQUIRED" here so we can handle missing GTest
# showing a better error message
find_package(GTest "1.7")

if(NOT GTEST_FOUND)
    message(STATUS "")
    message(STATUS "GTest package was not found in following folder")
    message(STATUS "  ${GTEST_ROOT}")
    message(STATUS "Please make sure that GTest was compiled in the above")
    message(STATUS "mentioned folder as follows:")
    message(STATUS "  shell> cd ${GTEST_ROOT}")
    message(STATUS "  shell> cmake .")
    message(STATUS "  shell> make")
    message(STATUS "Or set the environment variable GTEST_ROOT pointing")
    message(STATUS "to the installation of GTest.")
    message(STATUS "")

    message(FATAL_ERROR "Please fix GTest installation (see above message)")
endif()
