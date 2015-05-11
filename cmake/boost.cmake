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

# CMake module handling Boost Library dependency

# Note: downloading inspired by MySQL Server cmake/boost.cmake

# Check settings.cmake for configuring the package and minimum version
# of Boost.

include(CMakeParseArguments)

# Handle environment variables
if(DEFINED ENV{WITH_BOOST} AND NOT DEFINED WITH_BOOST)
    set(WITH_BOOST "$ENV{WITH_BOOST}")
endif()

if(DEFINED ENV{BOOST_ROOT} AND NOT DEFINED WITH_BOOST)
    SET(WITH_BOOST "$ENV{BOOST_ROOT}")
elseif(WITH_BOOST AND NOT DEFINED BOOST_ROOT)
    set(BOOST_ROOT ${WITH_BOOST})
endif()

# Package name is the folder when Boost archive is unpacked
string(REPLACE "." "_" BOOST_PACKAGE_NAME "boost.${BOOST_MINIMUM_VERSION}")
set(BOOST_TARBALL "${BOOST_PACKAGE_NAME}.tar.gz")
set(BOOST_DOWNLOAD_URL
  "http://sourceforge.net/projects/boost/files/boost/${BOOST_MINIMUM_VERSION}/${BOOST_TARBALL}"
  )
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Boost URL ${BOOST_DOWNLOAD_URL}")
endif()

function(BOOST_CHECK_OLD_PACKAGES)
    # This function only errors when an old packages when no current is available.
    # It will not fail when it can not find any Boost version.
    set(multiValueArgs WITH_BOOST)
    cmake_parse_arguments(FIND_BOOST "" "" "${multiValueArgs}" ${ARGN})
    file(GLOB old_packages "${WITH_BOOST}/boost_?_*_[0-9]")
    set(found_current FALSE)
    set(found_old "")
    foreach(pkg_path ${old_packages})
        get_filename_component(pkg ${pkg_path} NAME)
        message("pkg: ${pkg}")
        if(pkg STREQUAL ${BOOST_PACKAGE_NAME})
            set(found_current TRUE)
            break()
        else()
            set(found_old ${pkg})
            break()
        endif()
    endforeach()

    if(NOT found_current AND NOT found_old STREQUAL "")
        message(STATUS "")
        message(STATUS "You must upgrade to Boost ${BOOST_MINIMUM_VERSION}")
        message(STATUS "Hint: Use -DWITH_BOOST=/your/dir/boost -DDOWNLOAD_BOOST=YES")
        message(STATUS "")
        message(FATAL_ERROR "Found old Boost installation: ${found_old}")
    endif()
endfunction()

macro(BOOST_NOT_FOUND)
  boost_check_old_packages(WITH_BOOST ${WITH_BOOST})
  message(STATUS "Could not find Boost ${BOOST_MINIMUM_VERSION} or later.")
  message(FATAL_ERROR
    "You can download Boost with -DDOWNLOAD_BOOST=YES -DWITH_BOOST=<directory>. "
    "The downloaded Boost archive will be saved and unpacked in <directory>. "
    "If you are behind a firewall, you may need to use an HTTP proxy:\n"
    "    shell> export http_proxy=http://example.com:8080\n"
    "or https_proxy environment variable if SSL is required."
    )
endmacro()

function(FIND_BOOST)
    set(oneValueArgs MIN_VERSION)
    set(multiValueArgs PATHS)
    cmake_parse_arguments(FIND_BOOST "" "${oneValueArgs}" "${multiValueArgs}"
        ${ARGN})

    # Search for the version file, first in LOCAL_BOOST_DIR or WITH_BOOST
    find_path(include_dirs
        NAMES boost/version.hpp
        HINTS ${FIND_BOOST_PATHS}
        NO_DEFAULT_PATH
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT include_dirs)
        message(STATUS "Boost not found in ${FIND_BOOST_PATHS}")
    endif()

    # Then search in standard places (if not found)
    FIND_PATH(include_dirs
      NAMES boost/version.hpp
    )

    if(NOT include_dirs)
        message(STATUS
            "Looked for boost/version.hpp in ${FIND_BOOST_PATHS}")
        boost_not_found()
    else()
        if(CMAKE_BUILD_TYPE MATCHES "Debug")
            message(STATUS "Found ${include_dirs}/boost/version.hpp ")
        endif()
    endif()

    # The line from version.hpp containing version
    file(STRINGS "${include_dirs}/boost/version.hpp"
        version_line
        REGEX "^#define[\t ]+BOOST_VERSION[\t ][0-9]+.*"
    )
    string(REGEX MATCH "[0-9][0-9][0-9][0-9][0-9][0-9]" version_str "${version_line}")
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
        if(version_line)
            message(STATUS "Found Boost version string '${version_str}' in line '${version_line}'")
        else()
            message(STATUS "Failed parsing Boost version")
        endif()
    endif()

    if(NOT version_line)
        boost_not_found()
    endif()

    # Get a more useful version number
    math(EXPR ver_major "${version_str} / 100000")
    math(EXPR ver_minor "${version_str} / 100 % 1000")
    math(EXPR ver_patch "${version_str} % 100")
    set(version "${ver_major}.${ver_minor}.${ver_patch}")

    if(FIND_BOOST_MIN_VERSION)
        if(version VERSION_LESS FIND_BOOST_MIN_VERSION)
            message(SEND_ERROR "Boost ${version} found, but version ${BOOST_MINIMUM_VERSION} or later is required")
            boost_not_found()
            return()
        else()
            message(STATUS "Found Boost: ${include_dirs} (version ${version}; minimum required is ${FIND_BOOST_MIN_VERSION})")
        endif()
    else()
        message(STATUS "Found Boost: ${include_dirs} (version ${BOOST_VERSION}")
    endif(FIND_BOOST_MIN_VERSION)

    set(Boost_FOUND TRUE PARENT_SCOPE)
    set(Boost_INCLUDE_DIRS ${include_dirs} PARENT_SCOPE)
endfunction(FIND_BOOST)

# Try downloading Boost when we were allowed, but only if found Boost is not
# current enough.
if(DOWNLOAD_BOOST AND (Boost_VERSION VERSION_LESS BOOST_MINIMUM_VERSION OR NOT Boost_FOUND))
    if(Boost_FOUND)
        message(WARNING "Boost ${BOOST_VERSION} found, but version ${BOOST_MINIMUM_VERSION} or later is required")
    endif()
    if(NOT WITH_BOOST)
        boost_not_found()
    endif()

    message(STATUS "Downloading Boost in ${WITH_BOOST}")

    if(WITH_BOOST)
        # Handle path to archive
        if(${WITH_BOOST} MATCHES ".*\\.tar.gz" OR ${WITH_BOOST} MATCHES ".*\\.zip")
            get_filename_component(BOOST_DIR ${WITH_BOOST} PATH)
            get_filename_component(BOOST_ZIP ${WITH_BOOST} NAME)
            find_file(LOCAL_BOOST_ZIP
                NAMES ${BOOST_ZIP}
                PATHS ${BOOST_DIR}
                NO_DEFAULT_PATH
            )
        endif()

        # Handle folder
        find_file(LOCAL_BOOST_ZIP
            NAMES "${BOOST_PACKAGE_NAME}.tar.gz" "${BOOST_PACKAGE_NAME}.zip"
            PATHS ${WITH_BOOST}
            NO_DEFAULT_PATH
        )

        # Handle unpacked archive
        find_file(LOCAL_BOOST_DIR
            NAMES "${BOOST_PACKAGE_NAME}"
            PATHS ${WITH_BOOST}
            NO_DEFAULT_PATH
        )

        # Handle installation folder of Boost
        find_path(LOCAL_BOOST_DIR
            NAMES "boost/version.hpp"
            PATHS ${WITH_BOOST}
            NO_DEFAULT_PATH
        )

        if(LOCAL_BOOST_DIR)
            MESSAGE(STATUS "Local Boost folder ${LOCAL_BOOST_DIR}")
        elseif(LOCAL_BOOST_ZIP)
            MESSAGE(STATUS "Local Boost archive ${LOCAL_BOOST_ZIP}")
        endif()
    endif()

    # Boot tarball can be big
    set(DOWNLOAD_BOOST_TIMEOUT 600 CACHE STRING
        "Timeout in seconds when downloading Boost.")

    # If we could not find Boost, try downloading
    if(WITH_BOOST AND NOT LOCAL_BOOST_ZIP AND NOT LOCAL_BOOST_DIR)
        if(NOT DOWNLOAD_BOOST)
            message(WARNING "Not downloading")
            boost_not_found()
        endif()

        # Download the tarball
        message(STATUS "Downloading ${BOOST_TARBALL} to ${WITH_BOOST}")
        file(DOWNLOAD ${BOOST_DOWNLOAD_URL}
            ${WITH_BOOST}/${BOOST_TARBALL}
            TIMEOUT ${DOWNLOAD_BOOST_TIMEOUT}
            STATUS ERR
            SHOW_PROGRESS
        )

        if(ERR EQUAL 0)
            SET(LOCAL_BOOST_ZIP "${WITH_BOOST}/${BOOST_TARBALL}" CACHE INTERNAL "")
        else()
            message(STATUS "Download failed, error: ${ERR}")
            # A failed download leaves an empty file, remove it
            file(REMOVE ${WITH_BOOST}/${BOOST_TARBALL})
            # STATUS returns a list of length 2
            list(GET ERR 0 NUMERIC_RETURN)

            set(manual_msg
                "You can try downloading Boost manually using the following URL:\n"
                " ${BOOST_DOWNLOAD_URL}\n"
                )
            if(NUMERIC_RETURN EQUAL 28)
                message(FATAL_ERROR
                    "${manual_msg}\n"
                    "or increase the value of DOWNLOAD_BOOST_TIMEOUT "
                    "(which is now ${DOWNLOAD_BOOST_TIMEOUT} seconds)."
                )
            else()
                message(FATAL_ERROR "${manual_msg}")
            endif()

        endif()
    endif()

    # Unpack the archive
    if(LOCAL_BOOST_ZIP AND NOT LOCAL_BOOST_DIR)
        get_filename_component(LOCAL_BOOST_DIR ${LOCAL_BOOST_ZIP} PATH)
        if(NOT EXISTS "${LOCAL_BOOST_DIR}/${BOOST_PACKAGE_NAME}")
            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                message(STATUS "cd ${LOCAL_BOOST_DIR}; tar xfz ${LOCAL_BOOST_ZIP}")
            endif()
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xfz "${LOCAL_BOOST_ZIP}"
                WORKING_DIRECTORY "${LOCAL_BOOST_DIR}"
                RESULT_VARIABLE unpack_result
            )
            if(unpack_result MATCHES 0)
                set(BOOST_FOUND 1 CACHE INTERNAL "")
            else()
                if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                    message(STATUS "WITH_BOOST set to ${WITH_BOOST} after unpacking")
                endif()
                message(STATUS "Failed to extract files. "
                    "Please try downloading and extracting yourself. The url is:\n"
                    "${BOOST_DOWNLOAD_URL}\n")
                message(FATAL_ERROR "Aborting")
            endif(unpack_result MATCHES 0)
        endif()
    endif(LOCAL_BOOST_ZIP AND NOT LOCAL_BOOST_DIR)
endif()

# We are ready to find Boost
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS 
        "Looking for Boost in ${WITH_BOOST}/${BOOST_PACKAGE_NAME} ${WITH_BOOST}")
endif()

find_boost(MIN_VERSION ${BOOST_MINIMUM_VERSION}
    PATHS ${WITH_BOOST}/${BOOST_PACKAGE_NAME}
          ${WITH_BOOST})
