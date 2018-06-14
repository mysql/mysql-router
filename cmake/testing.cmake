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

set(_TEST_RUNTIME_DIR ${PROJECT_BINARY_DIR}/tests)

# We include GMock without touching the compile flags. GMock can
# handle that itself. It will also indirectly create targets for gmock
# and gtest.
#
# Two alternatives for locating GMock *source code*:
# 1. If WITH_GMOCK is given, this is expected to be the location of
#    the *source code*.
# 2. If WITH_GMOCK is not given, it will look in the 'ext' directory
#    in the source root.
if(ENABLE_TESTS)
  if(TARGET gmock)
    # don't build gmock, if the parent already built it

    # copying from unittest/gunit/CMakeFiles.txt
    # this should all be global-variables or a cmake/ file
    if(NOT DOWNLOAD_ROOT)
      set(DOWNLOAD_ROOT ${CMAKE_SOURCE_DIR}/source_downloads)
    endif()

    # We want googletest version 1.8, which also contains googlemock.
    set(GMOCK_PACKAGE_NAME "release-1.8.0")

    if(DEFINED ENV{WITH_GMOCK} AND NOT DEFINED WITH_GMOCK)
      file(TO_CMAKE_PATH "$ENV{WITH_GMOCK}" WITH_GMOCK)
    ENDIF()

    if(LOCAL_GMOCK_ZIP
       AND NOT ${LOCAL_GMOCK_ZIP} MATCHES ".*${GMOCK_PACKAGE_NAME}\\.zip")
     set(LOCAL_GMOCK_ZIP 0)
    endif()

    if(WITH_GMOCK)
      ## Did we get a full path name, including file name?
      if(${WITH_GMOCK} MATCHES ".*\\.zip")
        GET_FILENAME_COMPONENT(GMOCK_DIR ${WITH_GMOCK} PATH)
        GET_FILENAME_COMPONENT(GMOCK_ZIP ${WITH_GMOCK} NAME)
        FIND_FILE(LOCAL_GMOCK_ZIP
                  NAMES ${GMOCK_ZIP}
                  PATHS ${GMOCK_DIR}
                  NO_DEFAULT_PATH
                 )
      else()
        ## Did we get a path name to the directory of the .zip file?
        ## Check for both release-x.y.z.zip and googletest-release-x.y.z.zip
        FIND_FILE(LOCAL_GMOCK_ZIP
                  NAMES "${GMOCK_PACKAGE_NAME}.zip" "googletest-${GMOCK_PACKAGE_NAME}.zip"
                  PATHS ${WITH_GMOCK}
                  NO_DEFAULT_PATH
                  )
        ## If WITH_GMOCK is a directory, use it for download.
        set(DOWNLOAD_ROOT ${WITH_GMOCK})
      endif()
      MESSAGE(STATUS "Local gmock zip ${LOCAL_GMOCK_ZIP}")
    endif()

    set(GMOCK_SOURCE_DIR ${DOWNLOAD_ROOT}/googletest-${GMOCK_PACKAGE_NAME}/googlemock)
    set(GTEST_SOURCE_DIR ${DOWNLOAD_ROOT}/googletest-${GMOCK_PACKAGE_NAME}/googletest)

    # introduce some compat
    set(GTEST_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS})
    message("yyy seting GTEST_INCLUDE_DIRS to ${GTEST_INCLUDE_DIRS}")

    ADD_LIBRARY(gmock_main STATIC ${GMOCK_SOURCE_DIR}/src/gmock_main.cc)
    target_link_libraries(gmock_main gmock)
    target_include_directories(gmock_main
      PUBLIC ${GMOCK_INCLUDE_DIRS})
    ADD_LIBRARY(gtest_main STATIC ${GTEST_SOURCE_DIR}/src/gtest_main.cc)
    target_include_directories(gtest_main
      PUBLIC ${GMOCK_INCLUDE_DIRS})

    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      set_target_properties(gtest_main gmock_main
        PROPERTIES
        COMPILE_FLAGS "-Wno-undef -Wno-conversion")
    endif()

    set(TEST_LIBRARIES gmock gtest gmock_main gtest_main)
  else()
    if(WITH_GMOCK)

      # There is a known gtest/gmock bug that surfaces with the gcc-6.x causing tests crashes:
      # https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=833450
      # We have a patch for it in the gmock we bundle but if the user wants to use
      # it's own gtest/gmock we need to prevent it if the gcc-6.x is used
      if ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        AND (CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "6.0" OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "6.0"))
        message(FATAL_ERROR "Parameter WITH_GMOCK is not supported for gcc-6 or greater."
          "You need to either disable the tests or use the bundled gmock (removing WITH_GMOCK parameter).")
      endif()

      set(_gmock_root ${WITH_GMOCK})
      set(_gtest_root ${WITH_GMOCK}/gtest)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/ext/gmock/CMakeLists.txt")
      if(NOT EXISTS "${CMAKE_SOURCE_DIR}/ext/gtest/CMakeLists.txt")
        message(FATAL_ERROR "Cannot find GTest repository under ${CMAKE_SOURCE_DIR}/ext/gtest")
      endif()
      set(_gmock_root "${CMAKE_SOURCE_DIR}/ext/gmock")
      set(_gtest_root "${CMAKE_SOURCE_DIR}/ext/gtest")
    elseif(GMOCK_SOURCE_DIR)
      # means we are part of the server and GMOCK was downloaded
      set(_gmock_root ${GMOCK_SOURCE_DIR})
      set(_gtest_root ${GMOCK_SOURCE_DIR}/gtest)
    else()
      # means we are part of the server and GMOCK is missing
      # act as other server components, disable the tests
      SET (ENABLE_TESTS 0)
      SET (ENABLE_TESTS 0 PARENT_SCOPE)
    endif()

    if (ENABLE_TESTS)
      if(NOT EXISTS "${_gmock_root}/CMakeLists.txt")
        message(WARNING
          "Unable to find GMock source, not possible to build tests. Either "
          "disable tests with ENABLE_TESTS=no or download the source code "
          "for GMock (available at https://github.com/google/googlemock) and "
          "set WITH_GMOCK to the directory of the unpacked source code.")
      endif()

      message(STATUS "Found GMock source under ${_gmock_root}")
      add_subdirectory(${_gmock_root} ext/gmock)

      # Setting variables that are normally discovered using FindXXX.cmake
      set(GTEST_INCLUDE_DIRS ${_gtest_root}/include)
      message("yyy seting GTEST_INCLUDE_DIRS to ${GTEST_INCLUDE_DIRS}")
      set(GTEST_LIBRARIES gtest)
      set(GTEST_MAIN_LIBRARIES gtest_main)
      set(GTEST_BOTH_LIBRARIES ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})

      set(GMOCK_INCLUDE_DIRS ${_gmock_root}/include)
      set(GMOCK_LIBRARIES gmock)
      set(GMOCK_MAIN_LIBRARIES gmock_main)
      set(GMOCK_BOTH_LIBRARIES ${GMOCK_LIBRARIES} ${GMOCK_MAIN_LIBRARIES})

      set(TEST_LIBRARIES ${GMOCK_BOTH_LIBRARIES} ${GTEST_BOTH_LIBRARIES})

      # Since GMock and GTest do not set
      # INTERFACE_SYSTEM_INCLUDE_DIRECTORIES, we do that here. This means
      # that any targets that reference one of these libraries will
      # "automatically" have the include directories for these libraries
      # added to their build flags.  We cannot use "SYSTEM" since that is
      # not available in 2.8.9 (it was introduced in 2.8.12).
      target_include_directories(gmock PUBLIC ${GMOCK_INCLUDE_DIRS})
      target_include_directories(gmock_main PUBLIC ${GMOCK_INCLUDE_DIRS})
      target_include_directories(gtest PUBLIC ${GTEST_INCLUDE_DIRS})
      target_include_directories(gtest_main PUBLIC ${GTEST_INCLUDE_DIRS})

      if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

        set (comp_flags_ "-Wno-undef -Wno-missing-field-initializers")
        if(COMPILER_HAS_WARNING_MISSING_FORMAT_ATTRIBUTE)
          set(comp_flags_ "${comp_flags_} -Wno-missing-format-attribute")
        endif()

        set_target_properties(gtest gtest_main gmock gmock_main
          PROPERTIES
          COMPILE_FLAGS "${comp_flags_}")
      endif()
    endif()
  endif()
endif()

# Set {RUNTIME,LIBRARY}_OUTPUT_DIRECTORY properties of a target to the stage dir.
# On unix platforms this is just one directory, but on Windows it's per build-type,
# e.g. build/stage/Debug/lib, build/stage/Release/lib, etc
function(set_target_output_directory target target_output_directory dirname)
  if(NOT CMAKE_CFG_INTDIR STREQUAL ".")
    foreach(config_ ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${config_} config__)
      set_property(TARGET ${target} PROPERTY
        ${target_output_directory}_${config__} ${MySQLRouter_BINARY_STAGE_DIR}/${config_}/${dirname})
    endforeach()
  else()
    set_property(TARGET ${target} PROPERTY
      ${target_output_directory} ${MySQLRouter_BINARY_STAGE_DIR}/${dirname})
  endif()
endfunction()

# Prepare staging area
foreach(dir etc;run;log;bin;lib)
  if(NOT CMAKE_CFG_INTDIR STREQUAL ".")
    foreach(config_ ${CMAKE_CONFIGURATION_TYPES})
      file(MAKE_DIRECTORY ${MySQLRouter_BINARY_STAGE_DIR}/${config_}/${dir})
    endforeach()
  else()
    file(MAKE_DIRECTORY ${MySQLRouter_BINARY_STAGE_DIR}/${dir})
  endif()
endforeach()

function(add_test_file FILE)
  set(one_value_args MODULE LABEL ENVIRONMENT)
  set(multi_value_args LIB_DEPENDS INCLUDE_DIRS)
  cmake_parse_arguments(TEST "" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT TEST_MODULE)
    message(FATAL_ERROR "Module name missing for test file ${FILE}")
  endif()

  get_filename_component(test_ext ${FILE} EXT)
  get_filename_component(runtime_dir ${FILE} PATH)  # Not using DIRECTORY because of CMake >=2.8.11 requirement

  set(runtime_dir ${PROJECT_BINARY_DIR}/tests/${TEST_MODULE})

  if(test_ext STREQUAL ".cc")
    # Tests written in C++
    get_filename_component(test_target ${FILE} NAME_WE)
    string(REGEX REPLACE "^test_" "" test_target ${test_target})
    set(test_target "test_${TEST_MODULE}_${test_target}")
    set(test_name "tests/${TEST_MODULE}/${test_target}")
    add_executable(${test_target} ${FILE})
    target_link_libraries(${test_target}
      gtest gtest_main gmock gmock_main routertest_helpers
      router_lib harness-library
      ${CMAKE_THREAD_LIBS_INIT})
    foreach(libtarget ${TEST_LIB_DEPENDS})
      #add_dependencies(${test_target} ${libtarget})
      target_link_libraries(${test_target} ${libtarget})
    endforeach()
    foreach(include_dir ${TEST_INCLUDE_DIRS})
      target_include_directories(${test_target} PUBLIC ${include_dir})
    endforeach()
    set_target_properties(${test_target}
      PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${runtime_dir}/)
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      # silence undefined use of macro-vars in gtest.
      # we can't use #pragma's due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431 to
      # supress it locally.
      set_target_properties(
        ${test_target}
        PROPERTIES
        COMPILE_FLAGS "-Wno-undef -Wno-conversion")
    endif()

    if(WITH_VALGRIND)
      FIND_PROGRAM(VALGRIND valgrind)
      SET(TEST_WRAPPER ${VALGRIND} --error-exitcode=1)
    endif()

    add_test(NAME ${test_name}
      COMMAND ${TEST_WRAPPER} $<TARGET_FILE:${test_target}> --gtest_output=xml:${runtime_dir}/${test_target}.xml)

    SET(TEST_ENV_PREFIX "STAGE_DIR=${MySQLRouter_BINARY_STAGE_DIR};CMAKE_SOURCE_DIR=${MySQLRouter_SOURCE_DIR};CMAKE_BINARY_DIR=${MySQLRouter_BINARY_DIR}")
    if(WITH_VALGRIND)
      SET(TEST_ENV_PREFIX "${TEST_ENV_PREFIX};WITH_VALGRIND=1")
    endif()

    if (WIN32)
      # PATH's separator ";" needs to be escaped as CMAKE's test-env is also separated by ; ...
      STRING(REPLACE ";" "\\;" ESC_ENV_PATH "$ENV{PATH}")

      ## win32 has single and multi-configuration builds
      set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT
        "${TEST_ENV_PREFIX};PATH=$<TARGET_FILE_DIR:harness-library>\;$<TARGET_FILE_DIR:mysqlrouter>\;$<TARGET_FILE_DIR:mysql_protocol>\;$<TARGET_FILE_DIR:http_common>\;$<TARGET_FILE_DIR:duktape>\;${ESC_ENV_PATH};${TEST_ENVIRONMENT}")
    else()
      set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT
        "${TEST_ENV_PREFIX};LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH};DYLD_LIBRARY_PATH=$ENV{DYLD_LIBRARY_PATH};${TEST_ENVIRONMENT}")
    endif()
  else()
    message(ERROR "Unknown test type; file '${FILE}'")
  endif()

endfunction(add_test_file)

function(add_test_dir DIR_NAME)
  set(one_value_args MODULE ENVIRONMENT)
  set(multi_value_args LIB_DEPENDS INCLUDE_DIRS)
  cmake_parse_arguments(TEST "" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT TEST_MODULE)
    message(FATAL_ERROR "Module name missing for test folder ${DIR_NAME}")
  endif()

  get_filename_component(abs_path ${DIR_NAME} ABSOLUTE)

  file(GLOB test_files RELATIVE ${abs_path}
    ${abs_path}/*.cc)

  foreach(test_file ${test_files})
    if(NOT ${test_file} MATCHES "^helper")
      add_test_file(${abs_path}/${test_file}
        MODULE ${TEST_MODULE}
        ENVIRONMENT ${TEST_ENVIRONMENT}
        LIB_DEPENDS ${TEST_LIB_DEPENDS}
        INCLUDE_DIRS ${TEST_INCLUDE_DIRS}
        )
    endif()
  endforeach(test_file)

endfunction(add_test_dir)
