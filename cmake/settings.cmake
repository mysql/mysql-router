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

# Settings for building MySQL Router

# General
set(MYSQL_ROUTER_TARGET "mysqlrouter"
  CACHE STRING "Name of the MySQL Router application")  # Also used in CMAKE_INSTALL_PREFIX
set(MYSQL_ROUTER_NAME "MySQL Router"
  CACHE STRING "MySQL Router project name")
set(MYSQL_ROUTER_INI "mysqlrouter.ini"
  CACHE STRING "Name of default configuration file")

# Command line options for CMake
option(ENABLE_TESTS "Enable Tests" NO)
option(WITH_STATIC "Enable static linkage of external libraries" NO)
option(GPL "Produce GNU GPLv2 source and binaries" YES)

# MySQL Harness
set(HARNESS_NAME "mysqlrouter" CACHE STRING "Name of Harness")

