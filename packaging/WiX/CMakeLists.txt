# Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.
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

IF(NOT WIN32)
  RETURN()
ENDIF()

FIND_PATH(WIX_DIR NAMES heat.exe)

IF(NOT WIX_DIR)
  IF(NOT _WIX_DIR_CHECKED)
    SET(_WIX_DIR_CHECKED 1 CACHE INTERNAL "")
    MESSAGE(WARNING "Cannot find wix 3, installer project will not be generated")
  ENDIF()
  RETURN()
ENDIF()

ADD_SUBDIRECTORY(ca)

FIND_PROGRAM(HEAT_EXECUTABLE heat ${WIX_DIR})
FIND_PROGRAM(CANDLE_EXECUTABLE candle ${WIX_DIR})
FIND_PROGRAM(LIGHT_EXECUTABLE light ${WIX_DIR})

# WiX wants the license text as rtf; if there is no rtf license,
# we create a fake one from the plain text COPYING file.
IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.rtf")
 SET(COPYING_RTF "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.rtf")
ELSE()
  IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE.mysql")
    SET(LICENSE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE.mysql")
  ELSE()
    SET(LICENSE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../License.txt")
  ENDIF()
  FILE(READ ${LICENSE_FILE} CONTENTS)
  STRING(REGEX REPLACE "\n" "\\\\par\n" CONTENTS "${CONTENTS}")
  STRING(REGEX REPLACE "\t" "\\\\tab" CONTENTS "${CONTENTS}")
  FILE(WRITE "${CMAKE_CURRENT_BINARY_DIR}/COPYING.rtf" "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil\\fcharset0 Courier New;}}\\viewkind4\\uc1\\pard\\lang1031\\f0\\fs15")
  FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/COPYING.rtf" "${CONTENTS}")
  FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/COPYING.rtf" "\n}\n")
  SET(COPYING_RTF "${CMAKE_CURRENT_BINARY_DIR}/COPYING.rtf")
ENDIF()
SET(CPACK_WIX_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/CPackWixConfig.cmake)
SET(CPACK_WIX_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/mysql_router_extra.wxs;${CMAKE_CURRENT_SOURCE_DIR}/custom_ui.wxs")

IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(WixWin64 " Win64='yes'")
ELSE()
  SET(WixWin64 " Win64='no'")
ENDIF()

if(NOT CMAKE_CFG_INTDIR STREQUAL ".")
  foreach(conf ${CMAKE_CONFIGURATION_TYPES})
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysqlrouter.conf.in  ${PROJECT_BINARY_DIR}/stage/${conf}/etc/mysqlrouter.conf.sample)
  endforeach()
else()
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysqlrouter.conf.in  ${PROJECT_BINARY_DIR}/stage/etc/mysqlrouter.conf.sample)
endif()

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysql_router_extra.wxs.in
  ${CMAKE_CURRENT_BINARY_DIR}/mysql_router_extra.wxs)

## configure_file doesn't support generator expressions
SET(WIXCA_LOCATION "$<TARGET_FILE:wixca>")

if(NOT CMAKE_CFG_INTDIR STREQUAL ".")
  FILE(READ "${CMAKE_CURRENT_SOURCE_DIR}/create_msi.cmake.in" TMPL_CONTENT)
  STRING(CONFIGURE "${TMPL_CONTENT}" GEN_CONTENT @ONLY)
  FILE(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/create_msi.cmake"
    CONTENT "${GEN_CONTENT}")
else()
  FILE(READ "${CMAKE_CURRENT_SOURCE_DIR}/create_msi.cmake.in" TMPL_CONTENT)
  STRING(CONFIGURE "${TMPL_CONTENT}" GEN_CONTENT @ONLY)
  FILE(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake"
    CONTENT "${GEN_CONTENT}")
endif()


if(NOT CMAKE_CFG_INTDIR STREQUAL ".")
 SET(CONFIG_PARAM "-DCMAKE_INSTALL_CONFIG_NAME=${CMAKE_CFG_INTDIR}")
ENDIF()


ADD_CUSTOM_TARGET(
  MSI
  COMMAND set VS_UNICODE_OUTPUT=
  COMMAND ${CMAKE_COMMAND}
  ${CONFIG_PARAM}
  -P  ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/create_msi.cmake
)
ADD_DEPENDENCIES(MSI wixca)
