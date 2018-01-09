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

include_directories(
  ${PROJECT_SOURCE_DIR}/include/
)

add_subdirectory(src/)
if(ENABLE_TESTS)
  add_subdirectory(tests/)
endif()

if(INSTALL_DOCDIR)
  install(FILES ${PROJECT_SOURCE_DIR}/doc/sample_mysqlrouter.conf DESTINATION ${INSTALL_DOCDIR})
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/include/mysql.h")
    # only install the README, License and so on of we are not part of the server-tree
    if(NOT GPL)
      InstallCommercialReadmeLicense()
    endif()
    install(FILES ${PROJECT_SOURCE_DIR}/README.txt DESTINATION ${INSTALL_DOCDIR})
    install(FILES ${PROJECT_SOURCE_DIR}/License.txt DESTINATION ${INSTALL_DOCDIR})
    if(INSTALL_LAYOUT STREQUAL "DEFAULT" OR INSTALL_LAYOUT STREQUAL "STANDALONE")
      if(GPL)
        install(FILES ${PROJECT_SOURCE_DIR}/packaging/deb-common/mysql-router.mysqlrouter.init
          DESTINATION ${INSTALL_DOCDIR} RENAME sample_mysqlrouter.init)
      else()
        install(FILES ${PROJECT_SOURCE_DIR}/internal/packaging/deb-common-commercial/mysql-router-commercial.mysqlrouter.init
          DESTINATION ${INSTALL_DOCDIR} RENAME sample_mysqlrouter.init)
      endif()
    endif()
  endif()
endif()


# don't install headers until a) a final destination is found and b) API is stable
#
# file(GLOB router_headers include/mysqlrouter/*.h)
# install(FILES ${router_headers}
#   DESTINATION "include/mysql/${HARNESS_NAME}")

# Install logging, runtime, config and data folders
if(INSTALL_LAYOUT STREQUAL "DEFAULT" OR INSTALL_LAYOUT STREQUAL "STANDALONE")
  # if we are part of the mysql-server tree, LOGDIR, CONFIGDIR, ... won't be set
  # provide reasonable defaults
  if (NOT INSTALL_LOGDIR)
    SET(INSTALL_LOGDIR "log")
  ENDIF()
  if (NOT INSTALL_CONFIGDIR)
    SET(INSTALL_CONFIGDIR "etc")
  ENDIF()
  if (NOT INSTALL_DATADIR)
    SET(INSTALL_DATADIR "share/mysqlrouter")
  ENDIF()
  if (NOT INSTALL_RUNTIMEDIR)
    SET(INSTALL_RUNTIMEDIR "run")
  ENDIF()
  foreach(_dirvar LOGDIR RUNTIMEDIR CONFIGDIR DATADIR)
    set(_dir ${INSTALL_${_dirvar}})
    IF(NOT _dir)
      MESSAGE(FATAL_ERROR "INSTALL_${_dirvar} is empty.")
    ENDIF()
    set(_stage_dir ${MySQLRouter_BINARY_STAGE_DIR}/${_dir})
    file(MAKE_DIRECTORY ${_stage_dir})

    # create a empty directory in the install folder
    install(DIRECTORY DESTINATION "./${_dir}"
      DIRECTORY_PERMISSIONS
        OWNER_WRITE OWNER_READ OWNER_EXECUTE
        GROUP_WRITE GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
      )
  endforeach()

  if(WIN32)
    # In commercial ZIP packages we distribute the redistributable VC++ runtime
    # But that can be done only in release builds (not debug).
    # (Debug builds ZIP packages does not include it)
   if((NOT GPL) AND INSTALL_LAYOUT STREQUAL "STANDALONE")
     if(NOT(DMYSQL_BUILD STREQUAL "debug"))
       message(STATUS "Gathering VC++ redistributable")
       include(InstallRequiredSystemLibraries)
     else()
       message(WARNING "No VC++ runtime will be redistributed for Debug builds")
     endif()
    endif()
  endif()
endif()
