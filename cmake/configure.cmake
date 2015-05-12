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

set(VERSION ${MySQLRouter_VERSION})
set(VERSION_MAJOR ${MySQLRouter_VERSION_MAJOR})
set(VERSION_MINOR ${MySQLRouter_VERSION_MINOR})
set(VERSION_PATCH ${MySQLRouter_VERSION_PATCH})
set(CONFIG_FILES ${CONFIG_FILE_LOCATIONS})

# Generate the copyright string
function(SET_COPYRIGHT TARGET)
  string(TIMESTAMP curr_year "%Y" UTC)
  set(start_year "2015")
  set(years "${start_year},")
  if(NOT curr_year STREQUAL ${start_year})
    set(years "${start_year}, ${curr_year}")
  endif()
  set(${TARGET} "Copyright (c) ${years} Oracle and/or its affiliates. All rights reserved." PARENT_SCOPE)
endfunction()

set_copyright(ORACLE_COPYRIGHT)

configure_file(config.h.in config.h)

include_directories(${PROJECT_BINARY_DIR})
