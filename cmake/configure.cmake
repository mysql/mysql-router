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

include(GNUInstallDirs)

# Configuration folder (config_folder configuration option)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(_configdir "ENV{APPDATA}")
else()
  if(IS_ABSOLUTE ${INSTALL_CONFIGDIR})
    set(_configdir ${INSTALL_CONFIGDIR})
  elseif(${INSTALL_CONFIGDIR} STREQUAL ".")
    # Current working directory
    set(_configdir ${INSTALL_CONFIGDIR})
  else()
    set(_configdir ${CMAKE_INSTALL_PREFIX}/${INSTALL_CONFIGDIR})
  endif()
endif()
set(ROUTER_CONFIGDIR ${_configdir} CACHE STRING "Location of configuration file(s) (config_folder)")
unset(_configdir)

# Logging folder (logging_folder configuration option)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(_logdir "ENV{APPDATA}\\log")
else()
  # logging folder can be set to empty to log to console
  if(IS_ABSOLUTE "${INSTALL_LOGDIR}" OR NOT INSTALL_LOGDIR)
    set(_logdir ${INSTALL_LOGDIR})
  else()
    set(_logdir ${CMAKE_INSTALL_PREFIX}/${INSTALL_LOGDIR})
  endif()
endif()
set(ROUTER_LOGDIR ${_logdir} CACHE STRING "Location of log files; empty is console (logging_folder)")
unset(_logdir)

# Runtime folder (runtime_folder configuration option)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(_runtimedir "ENV{APPDATA}")
else()
  if(IS_ABSOLUTE "${INSTALL_RUNTIMEDIR}")
    set(_runtimedir ${INSTALL_RUNTIMEDIR})
  else()
    set(_runtimedir ${CMAKE_INSTALL_PREFIX}/${INSTALL_RUNTIMEDIR})
  endif()
endif()
set(ROUTER_RUNTIMEDIR ${_runtimedir} CACHE STRING "Location runtime files such as PID file (runtime_folder)")
unset(_runtimedir)

# Plugin folder (plugin_folder configuration option)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(_plugindir "ENV{APPDATA}")
else()
  if(IS_ABSOLUTE "${INSTALL_PLUGINDIR}")
    set(_plugindir ${INSTALL_PLUGINDIR})
  else()
    set(_plugindir ${CMAKE_INSTALL_PREFIX}/${INSTALL_PLUGINDIR})
  endif()
endif()
set(ROUTER_PLUGINDIR ${_plugindir} CACHE STRING "Location MySQL Router plugins (plugin_folder)")
unset(_plugindir)

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

# Default configuration file locations (similar to MySQL Server)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(ver "${MySQLRouter_VERSION_MAJOR}.${MySQLRouter_VERSION_MINOR}")
  file(TO_NATIVE_PATH ${CMAKE_INSTALL_PREFIX} install_prefix)
  # We are using Raw strings (see config.h.in), no double escaping of \\ needed
  set(CONFIG_FILE_LOCATIONS
      "${SYSCONFDIR}\\${MYSQL_ROUTER_INI}"
      )
  unset(ver)
  unset(install_prefix)
else()
  set(CONFIG_FILE_LOCATIONS
      "${ROUTER_CONFIGDIR}/${MYSQL_ROUTER_INI}"
      "ENV{HOME}/.${MYSQL_ROUTER_INI}"
      )
endif()
set(CONFIG_FILES ${CONFIG_FILE_LOCATIONS})

if(INSTALL_LAYOUT STREQUAL "STANDALONE")
  set(ROUTER_PLUGINDIR "{origin}/../${INSTALL_PLUGINDIR_STANDALONE}")
  set(ROUTER_CONFIGDIR "{origin}/../${INSTALL_CONFIGDIR_STANDALONE}")
  set(ROUTER_RUNTIMEDIR "{origin}/../${INSTALL_RUNTIMEDIR_STANDALONE}")
endif()

configure_file(config.h.in config.h @ONLY)
include_directories(${PROJECT_BINARY_DIR})
