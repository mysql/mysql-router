# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

# MS Visual C++ specifics
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)  # bypass Warning C4996 (getenv)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/compile_flags.cmake)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include/mysqlrouter
  ${SSL_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/ext/rapidjson/include)

if(WITH_SSL STREQUAL "bundled")
  set(MY_AES_IMPL common/my_aes_yassl.cc)
  add_definitions(-DSIZEOF_LONG=${SIZEOF_LONG} -DSIZEOF_LONG_LONG=${SIZEOF_LONG_LONG})
else()
  set(MY_AES_IMPL common/my_aes_openssl.cc)
endif()

add_definitions(${SSL_DEFINES})


set(lib_source_files
  config_generator.cc
  cluster_metadata.cc
  router_app.cc
  utils.cc
  uri.cc
  datatypes.cc
  plugin_config.cc
  common/my_aes.cc
  common/my_sha1.cc
  common/mysql_session.cc
  common/utils_sqlstring.cc
  ${MY_AES_IMPL})
if(WIN32)
  list(APPEND lib_source_files windows/nt_servc.cc windows/main-windows.cc windows/password_vault.cc)
endif()

# Disable warnings from 3rd party code that we have no control over
if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  add_compile_flags(common/my_aes.cc common/my_sha1.cc ${MY_AES_IMPL} COMPILE_FLAGS
      -Wno-sign-conversion
      -Wno-unused-parameter
      -Wno-conversion)
endif()

set(source_files main.cc)

if(NOT WIN32)
  add_library(router_lib SHARED ${lib_source_files} ${common_files})
else()
  # on Windows, we statically link the router lib to otherwise avoid exporting a bunch of symbols
  add_library(router_lib STATIC ${lib_source_files})
endif()
target_link_libraries(router_lib ${CMAKE_DL_LIBS} harness-library ${MySQL_LIBRARIES} ${SSL_LIBRARIES})
if(WIN32)
  target_link_libraries(router_lib crypt32)
endif()
target_include_directories(router_lib PUBLIC ../include ${MySQL_INCLUDE_DIRS})
if(WITH_HARNESS)
  target_include_directories(router_lib PUBLIC "${CMAKE_BINARY_DIR}")
  target_include_directories(router_lib PUBLIC "${CMAKE_BINARY_DIR}/include/mysql/harness")
  target_include_directories(router_lib PUBLIC "${CMAKE_BINARY_DIR}/include")
  target_include_directories(router_lib PUBLIC "${CMAKE_BINARY_DIR}/harness")
endif()
set_target_output_directory(router_lib LIBRARY_OUTPUT_DIRECTORY lib)
if(NOT WIN32)
  set_target_properties(router_lib PROPERTIES
    OUTPUT_NAME "mysqlrouter"
    SOVERSION 1
  )
else()
  set_target_properties(router_lib PROPERTIES
    OUTPUT_NAME "mysqlrouter_lib"
    SOVERSION 1
  )
endif()

add_executable(${MYSQL_ROUTER_TARGET} ${source_files})
target_link_libraries(${MYSQL_ROUTER_TARGET} ${CMAKE_DL_LIBS} router_lib harness-library)
if(WIN32)
  target_link_libraries(${MYSQL_ROUTER_TARGET} crypt32)
endif()
if(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
  target_link_libraries(${MYSQL_ROUTER_TARGET} -lnsl -lsocket)
endif()

set_target_output_directory(${MYSQL_ROUTER_TARGET} RUNTIME_OUTPUT_DIRECTORY bin)

if(WIN32)
  set(MYSQL_DLL ${MySQL_LIBRARIES})
  string(REPLACE ".lib" ".dll" MYSQL_DLL ${MYSQL_DLL})
  get_filename_component(MYSQL_FILE ${MYSQL_DLL} NAME)
  get_filename_component(MYSQL_DLL ${MYSQL_DLL} DIRECTORY)

  # Copies the dll to the binary location to enable debugging
  add_custom_command(TARGET ${MYSQL_ROUTER_TARGET} PRE_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy
                        "${MYSQL_DLL}/${MYSQL_FILE}" $<TARGET_FILE_DIR:${MYSQL_ROUTER_TARGET}>/${MYSQL_FILE})
  install(FILES "${MYSQL_DLL}/${MYSQL_FILE}" DESTINATION bin)
  IF (NOT GPL AND WITH_SSL_PATH)
    GET_FILENAME_COMPONENT(CRYPTO_NAME "${CRYPTO_LIBRARY}" NAME_WE)
    GET_FILENAME_COMPONENT(OPENSSL_NAME "${OPENSSL_LIBRARY}" NAME_WE)
    FILE(GLOB HAVE_CRYPTO_DLL "${WITH_SSL_PATH}/bin/${CRYPTO_NAME}.dll")
    FILE(GLOB HAVE_OPENSSL_DLL "${WITH_SSL_PATH}/bin/${OPENSSL_NAME}.dll")
    IF (HAVE_CRYPTO_DLL AND HAVE_OPENSSL_DLL)
      install(FILES "${HAVE_CRYPTO_DLL}" DESTINATION bin)
      install(FILES "${HAVE_OPENSSL_DLL}" DESTINATION bin)
    ENDIF()
  ENDIF()

endif()

COPY_OPENSSL_DLLS(${MYSQL_ROUTER_TARGET} $<TARGET_FILE_DIR:${MYSQL_ROUTER_TARGET}>)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(${MYSQL_ROUTER_TARGET} PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup")
elseif(CMAKE_COMPILER_IS_GNUCXX)
endif()

# For testing
if(ENABLE_TESTS)
  file(GLOB config_files ${CMAKE_CURRENT_SOURCE_DIR}/../tests/*.conf.in)
  if(NOT WIN32)
    set(ROUTER_STAGE_DIR ${CMAKE_BINARY_DIR}/stage)
    foreach(config_file ${config_files})
      get_filename_component(dest_config_file ${config_file} NAME)
      string(REGEX REPLACE ".in$" "" dest_config_file ${dest_config_file})
      configure_file(${config_file} ${STAGE_DIR}/etc/${dest_config_file})
    endforeach()
  else()
    foreach(conf ${CMAKE_CONFIGURATION_TYPES})
      set(ROUTER_STAGE_DIR ${CMAKE_BINARY_DIR}/stage/${conf})
      foreach(config_file ${config_files})
        get_filename_component(dest_config_file ${config_file} NAME)
        string(REGEX REPLACE ".in$" "" dest_config_file ${dest_config_file})
        configure_file(${config_file} ${STAGE_DIR}/${conf}/etc/${dest_config_file})
      endforeach()
    endforeach()
  endif()
endif()

install(TARGETS ${MYSQL_ROUTER_TARGET} router_lib
  RUNTIME DESTINATION ${INSTALL_BINDIR}
  LIBRARY DESTINATION ${INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${INSTALL_LIBDIR})
