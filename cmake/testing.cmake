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

set(_TEST_RUNTIME_DIR ${PROJECT_BINARY_DIR}/tests)

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
      SET(TEST_WRAPPER "valgrind --exit-code=1")
    endif()

    add_test(NAME ${test_name}
	    COMMAND ${TEST_WRAPPER} $<TARGET_FILE:${test_target}> --gtest_output=xml:${runtime_dir}/${test_target}.xml)

    SET(TEST_ENV_PREFIX "STAGE_DIR=${MySQLRouter_BINARY_STAGE_DIR};CMAKE_SOURCE_DIR=${MySQLRouter_SOURCE_DIR};CMAKE_BINARY_DIR=${MySQLRouter_BINARY_DIR}")

    if (WIN32)
      # PATH's separator ";" needs to be escaped as CMAKE's test-env is also separated by ; ...
      STRING(REPLACE ";" "\\;" ESC_ENV_PATH "$ENV{PATH}")

      ## win32 has single and multi-configuration builds
      set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT
	"${TEST_ENV_PREFIX};PATH=$<TARGET_FILE_DIR:harness-library>\;$<TARGET_FILE_DIR:mysqlrouter>\;$<TARGET_FILE_DIR:mysql_protocol>\;${ESC_ENV_PATH};${TEST_ENVIRONMENT}")
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
