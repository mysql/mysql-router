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

# Version information of MySQL Router

# Change following when releasing
set(PROJECT_VERSION_TEXT "0.1.0")
set(PROJECT_VERSION_LEVEL "dev")  # alpha, beta, rc or GA
set(PROJECT_PARTOF_TEXT "MySQL Fabric 1.6")

# Nothing hereunder needs change when releasing

# The GA level is actually not shown
if(PROJECT_VERSION_LEVEL STREQUAL "GA")
    set(PROJECT_VERSION_LEVEL "")
endif()

# Error out when version level is incorrect
set(VALID_EDITIONS "dev" "alpha" "beta" "rc" "")
list(FIND VALID_EDITIONS "${PROJECT_VERSION_LEVEL}" index)
if(index EQUAL -1)
    message(FATAL_ERROR "Incorrect version level, was '${PROJECT_VERSION_LEVEL}'" )
endif()
