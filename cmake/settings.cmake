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

# Settings for building MySQL Router

# General
set(MYSQL_PROJECT_NAME "MySQLRouter"
    CACHE STRING "MySQL Router Project name")
set(MYSQL_ROUTER_TARGET "mysqlrouter"
    CACHE STRING "Name of the MySQL Router application")

# Command line options for CMake
option(ENABLE_TESTS "Enable Tests" NO)
option(DOWNLOAD_BOOST "Download Boost C++ Libraries" NO)

# Boost Libraries
set(BOOST_MINIMUM_VERSION "1.58.0"
    CACHE STRING "Boost Libraries mimimum required version")
set(WITH_BOOST ${WITH_BOOST} CACHE PATH
  "Path to Boost installation")

# Python
set(PYTHON_MINIMUM_VERSION "2.7"
    CACHE STRING "Python mimimum required version")

# Google Test
set(GTEST_MINIMUM_VERSION "1.7.0"
    CACHE STRING "Google C++ Testing Framework minimum required version")
set(GTEST_DOWNLOAD_URL
    "http://googletest.googlecode.com/files/gtest-${GTEST_MINIMUM_VERSION}.zip"
    CACHE STRING "Google C++ Testing Framework download URL")
set(GTEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/gtest"
    CACHE STRING "Google C++ Testing Framework installation")
set(GTEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/gtest-source"
    CACHE STRING "Google C++ Testing Framework source (instead of download)")