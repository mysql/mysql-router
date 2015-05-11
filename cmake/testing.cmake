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

set(_TEST_RUNTIME_DIR ${CMAKE_BINARY_DIR}/tests)

# We make sure the tests/__init__.py is available for running tests
file(COPY ${CMAKE_SOURCE_DIR}/tests/__init__.py DESTINATION ${CMAKE_BINARY_DIR}/tests/)

function(ADD_TEST_FILE FILE)
    set(oneValueArgs MODULE LABEL)
    cmake_parse_arguments(TEST "" "${oneValueArgs}" "" ${ARGN})

    if(NOT TEST_MODULE)
        message(FATAL_ERROR "Module name missing for test file ${FILE}")
    endif()

    get_filename_component(test_ext ${FILE} EXT)
    get_filename_component(runtime_dir ${FILE} DIRECTORY)

    set(runtime_dir ${CMAKE_BINARY_DIR}/tests/${TEST_MODULE})

    if(test_ext STREQUAL ".cc")
        # Tests written in C++
        get_filename_component(test_target ${FILE} NAME_WE)
        set(test_name "tests/${TEST_MODULE}/${test_target}")
        add_executable(${test_target} ${FILE})
        target_include_directories(${test_target} PRIVATE
            ${Boost_INCLUDE_DIRS}
            ${GTEST_INCLUDE_DIRS})
        target_link_libraries(${test_target}
            ${GTEST_BOTH_LIBRARIES}
            ${CMAKE_THREAD_LIBS_INIT})
        set_target_properties(${test_target}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${runtime_dir}/)
        add_test(NAME ${test_name}
            COMMAND ${runtime_dir}/${test_target})
    elseif(test_ext STREQUAL ".py")
        # Tests written in Python
        get_filename_component(test_target ${FILE} NAME)
        set(test_name "tests/${TEST_MODULE}/${test_target}")
        add_test(NAME ${test_name}
            COMMAND ${PYTHON_EXECUTABLE} -B ${runtime_dir}/${test_target})

        file(COPY ${FILE} DESTINATION ${runtime_dir}/)
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT "CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR};CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR};PYTHONPATH=..")
    else()
        message(ERROR "Unknown test type; file '${FILE}'")
    endif()

endfunction(ADD_TEST_FILE)

function(ADD_TEST_DIR DIR_NAME)
    set(oneValueArgs MODULE)
    cmake_parse_arguments(TEST "" "${oneValueArgs}" "" ${ARGN})

    if(NOT TEST_MODULE)
        message(FATAL_ERROR "Module name missing for test folder ${DIR_NAME}")
    endif()

    get_filename_component(abs_path ${DIR_NAME} ABSOLUTE)

    file(GLOB_RECURSE test_files RELATIVE ${abs_path}
        ${abs_path}/*.cc
        ${abs_path}/*.py)

    foreach(test_file ${test_files})
        ADD_TEST_FILE(${abs_path}/${test_file} MODULE ${TEST_MODULE})
    endforeach(test_file)

endfunction(ADD_TEST_DIR)
