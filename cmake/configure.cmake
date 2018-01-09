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

include(GNUInstallDirs)

# installed executable location (used in config.h)
if(IS_ABSOLUTE "${INSTALL_BINDIR}")
  set(ROUTER_BINDIR ${INSTALL_BINDIR})
else()
  set(ROUTER_BINDIR ${CMAKE_INSTALL_PREFIX}/${INSTALL_BINDIR})
endif()

# Configuration folder (config_folder configuration option)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(_configdir "ENV{APPDATA}")
else()
  if(IS_ABSOLUTE ${INSTALL_CONFIGDIR})
    set(_configdir ${INSTALL_CONFIGDIR})
  elseif(INSTALL_CONFIGDIR STREQUAL ".")
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
  set(_logdir "ENV{APPDATA}\\\\log")
else()
  # logging folder can be set to empty to log to console
  if(IS_ABSOLUTE "${INSTALL_LOGDIR}")
    set(_logdir ${INSTALL_LOGDIR})
  elseif(NOT INSTALL_LOGDIR)
    set(_logdir "/var/log/mysqlrouter/")
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
  elseif(NOT INSTALL_RUNTIMEDIR)
    set(_logdir "/var/run/mysqlrouter/")
  else()
    set(_runtimedir ${CMAKE_INSTALL_PREFIX}/${INSTALL_RUNTIMEDIR})
  endif()
endif()
set(ROUTER_RUNTIMEDIR ${_runtimedir} CACHE STRING "Location runtime files such as PID file (runtime_folder)")
unset(_runtimedir)

# Plugin folder (plugin_folder configuration option)
if(IS_ABSOLUTE "${INSTALL_PLUGINDIR}")
  set(_plugindir ${INSTALL_PLUGINDIR})
else()
  set(_plugindir ${CMAKE_INSTALL_PREFIX}/${INSTALL_PLUGINDIR})
endif()
set(ROUTER_PLUGINDIR ${_plugindir} CACHE STRING "Location MySQL Router plugins (plugin_folder)")
unset(_plugindir)

# Data folder (data_folder configuration option)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(_datadir "ENV{APPDATA}\\\\data")
else()
  if(IS_ABSOLUTE "${INSTALL_DATADIR}")
    set(_datadir ${INSTALL_DATADIR})
  elseif(NOT INSTALL_DATADIR)
    set(_datadir "/var/lib/mysqlrouter/")
  else()
    set(_datadir "${CMAKE_INSTALL_PREFIX}/${INSTALL_DATADIR}")
  endif()
endif()
set(ROUTER_DATADIR ${_datadir} CACHE STRING "Location of data files such as keyring file")
unset(_datadir)


# Generate the copyright string
function(SET_COPYRIGHT TARGET)
  string(TIMESTAMP curr_year "%Y" UTC)
  set(start_year "2015")
  set(years "${start_year},")
  if(NOT curr_year STREQUAL ${start_year})
    set(years "${start_year}, ${curr_year},")
  endif()
  set(${TARGET} "Copyright (c) ${years} Oracle and/or its affiliates. All rights reserved." PARENT_SCOPE)
endfunction()

set_copyright(ORACLE_COPYRIGHT)

if(INSTALL_LAYOUT STREQUAL "STANDALONE")
  set(ROUTER_PLUGINDIR "{origin}/../${INSTALL_PLUGINDIR_STANDALONE}")
  set(ROUTER_CONFIGDIR "{origin}/../${INSTALL_CONFIGDIR_STANDALONE}")
  set(ROUTER_RUNTIMEDIR "{origin}/../${INSTALL_RUNTIMEDIR_STANDALONE}")
  set(ROUTER_LOGDIR "{origin}/../${INSTALL_LOGDIR_STANDALONE}")
  set(ROUTER_DATADIR "{origin}/../${INSTALL_DATADIR_STANDALONE}")
endif()

# Default configuration file locations (similar to MySQL Server)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(CONFIG_FILE_LOCATIONS
      "${ROUTER_CONFIGDIR}/${MYSQL_ROUTER_INI}"
      "ENV{APPDATA}/${MYSQL_ROUTER_INI}"
      )
else()
  set(CONFIG_FILE_LOCATIONS
      "${ROUTER_CONFIGDIR}/${MYSQL_ROUTER_INI}"
      "ENV{HOME}/.${MYSQL_ROUTER_INI}"
      )
endif()
set(CONFIG_FILES ${CONFIG_FILE_LOCATIONS})

# Platform/Compiler checks
INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

INCLUDE(CheckTypeSize)
CHECK_TYPE_SIZE("void *"    SIZEOF_VOIDP)
CHECK_TYPE_SIZE("char *"    SIZEOF_CHARP)
CHECK_TYPE_SIZE("long"      SIZEOF_LONG)
CHECK_TYPE_SIZE("short"     SIZEOF_SHORT)
CHECK_TYPE_SIZE("int"       SIZEOF_INT)
CHECK_TYPE_SIZE("long long" SIZEOF_LONG_LONG)
CHECK_TYPE_SIZE("off_t"     SIZEOF_OFF_T)
CHECK_TYPE_SIZE("time_t"    SIZEOF_TIME_T)

# Platform/Compiler checks
INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

# Compiler specific features
INCLUDE(CheckCSourceCompiles)
CHECK_C_SOURCE_COMPILES("
void test(const char *format, ...) __attribute__((format(printf, 1, 2)));
int main() {
  return 0;
}" HAVE_ATTRIBUTE_FORMAT)

MACRO(DIRNAME IN OUT)
  GET_FILENAME_COMPONENT(${OUT} ${IN} PATH)
ENDMACRO()

MACRO(FIND_REAL_LIBRARY SOFTLINK_NAME REALNAME)
  # We re-distribute libstdc++.so which is symlink.
  # There is no 'readlink' on solaris, so we use perl to follow the link:
  SET(PERLSCRIPT
    "my $link= $ARGV[0]; use Cwd qw(abs_path); my $file = abs_path($link); print $file;")
  EXECUTE_PROCESS(
    COMMAND perl -e "${PERLSCRIPT}" ${SOFTLINK_NAME}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE real_library
    )
  SET(REALNAME ${real_library})
ENDMACRO()

MACRO(EXTEND_CXX_LINK_FLAGS LIBRARY_PATH)
  # Using the $ORIGIN token with the -R option to locate the libraries
  # on a path relative to the executable:
  SET(CMAKE_CXX_LINK_FLAGS
    "${CMAKE_CXX_LINK_FLAGS} -R'\$ORIGIN/../lib' -R${LIBRARY_PATH}")
  MESSAGE(STATUS "CMAKE_CXX_LINK_FLAGS ${CMAKE_CXX_LINK_FLAGS}")
ENDMACRO()

MACRO(EXTEND_C_LINK_FLAGS LIBRARY_PATH)
  SET(CMAKE_C_LINK_FLAGS
    "${CMAKE_C_LINK_FLAGS} -R'\$ORIGIN/../lib' -R${LIBRARY_PATH}")
  MESSAGE(STATUS "CMAKE_C_LINK_FLAGS ${CMAKE_C_LINK_FLAGS}")
  SET(CMAKE_SHARED_LIBRARY_C_FLAGS
    "${CMAKE_SHARED_LIBRARY_C_FLAGS} -R'\$ORIGIN/../lib' -R${LIBRARY_PATH}")
ENDMACRO()

IF(CMAKE_SYSTEM_NAME MATCHES "SunOS" AND CMAKE_COMPILER_IS_GNUCC)
  DIRNAME(${CMAKE_CXX_COMPILER} CXX_PATH)
  SET(LIB_SUFFIX "lib")
  IF(SIZEOF_VOIDP EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
    SET(LIB_SUFFIX "lib/sparcv9")
  ENDIF()
  IF(SIZEOF_VOIDP EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
    SET(LIB_SUFFIX "lib/amd64")
  ENDIF()
  FIND_LIBRARY(GPP_LIBRARY_NAME
    NAMES "stdc++"
    PATHS ${CXX_PATH}/../${LIB_SUFFIX}
    NO_DEFAULT_PATH
  )
  MESSAGE(STATUS "GPP_LIBRARY_NAME ${GPP_LIBRARY_NAME}")
  IF(GPP_LIBRARY_NAME)
    DIRNAME(${GPP_LIBRARY_NAME} GPP_LIBRARY_PATH)
    FIND_REAL_LIBRARY(${GPP_LIBRARY_NAME} real_library)
    MESSAGE(STATUS "INSTALL ${GPP_LIBRARY_NAME} ${real_library}")
    INSTALL(FILES ${GPP_LIBRARY_NAME} ${real_library}
            DESTINATION ${INSTALL_LIBDIR} COMPONENT SharedLibraries)
    EXTEND_CXX_LINK_FLAGS(${GPP_LIBRARY_PATH})
    EXECUTE_PROCESS(
      COMMAND sh -c "elfdump ${real_library} | grep SONAME"
      RESULT_VARIABLE result
      OUTPUT_VARIABLE sonameline
    )
    IF(NOT result)
      STRING(REGEX MATCH "libstdc.*[^\n]" soname ${sonameline})
      MESSAGE(STATUS "INSTALL ${GPP_LIBRARY_PATH}/${soname}")
      INSTALL(FILES "${GPP_LIBRARY_PATH}/${soname}"
              DESTINATION ${INSTALL_LIBDIR} COMPONENT SharedLibraries)
    ENDIF()
  ENDIF()
  FIND_LIBRARY(GCC_LIBRARY_NAME
    NAMES "gcc_s"
    PATHS ${CXX_PATH}/../${LIB_SUFFIX}
    NO_DEFAULT_PATH
  )
  IF(GCC_LIBRARY_NAME)
    DIRNAME(${GCC_LIBRARY_NAME} GCC_LIBRARY_PATH)
    FIND_REAL_LIBRARY(${GCC_LIBRARY_NAME} real_library)
    MESSAGE(STATUS "INSTALL ${GCC_LIBRARY_NAME} ${real_library}")
    INSTALL(FILES ${GCC_LIBRARY_NAME} ${real_library}
            DESTINATION ${INSTALL_LIBDIR} COMPONENT SharedLibraries)
    EXTEND_C_LINK_FLAGS(${GCC_LIBRARY_PATH})
  ENDIF()
ENDIF()

# check if platform supports prlimit()
include(CMakePushCheckState)
cmake_push_check_state()
cmake_reset_check_state()
include(CheckSymbolExists)
set(CMAKE_REQUIRED_FLAGS -D_GNU_SOURCE)
check_symbol_exists(prlimit sys/resource.h HAVE_PRLIMIT)
cmake_pop_check_state()

configure_file(config.h.in router_config.h @ONLY)
include_directories(${PROJECT_BINARY_DIR})
