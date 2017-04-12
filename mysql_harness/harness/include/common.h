/*
  Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_COMMON_INCLUDED
#define MYSQL_HARNESS_COMMON_INCLUDED

#include <string>
#include <sstream>
#include <cstdlib>
#include "harness_export.h"

/**
 * @defgroup Various operations
 *
 * This module contain various utility operations.
 */

namespace mysql_harness {

/**
 * Deleter for smart pointers pointing to objects allocated with `std::malloc`.
 */
template<typename T>
class StdFreeDeleter {
 public:
  void operator()(T* ptr) {
    std::free(ptr);
  }
};

/**
 * Changes file access permissions to be fully accessible by all users.
 *
 * On Unix, the function sets file permission mask to 777.
 * On Windows, Everyone group is granted full access to the file.
 *
 * @param[in] file_name File name.
 *
 * @except std::exception Failed to change file permissions.
 */
void HARNESS_EXPORT make_file_public(const std::string& file_name);

/**
 * Changes file access permissions to be accessible only by a limited set of
 * users.
 *
 * On Unix, the function sets file permission mask to 600.
 * On Windows, all permissions to this file are removed for Everyone group.
 *
 * @param[in] file_name File name.
 *
 * @except std::exception Failed to change file permissions.
 */
void HARNESS_EXPORT make_file_private(const std::string& file_name);

/** @brief Wrapper for thread safe function returning error string.
 *
 * @param err error number
 * @return string describing the error
 */
std::string HARNESS_EXPORT get_strerror(int err);

/** @brief Rename a thread (useful for debugging purposes).
 *
 * @param thread_name thread name, 15 chars max
 */
void HARNESS_EXPORT rename_thread(const char thread_name[16]);

}

/**
 * Macros for disabling and enabling compiler warnings.
 *
 * The primary use case for these macros is suppressing warnings coming from
 * system and 3rd-party libraries' headers included in our code. It should
 * not be used to hide warnings in our code.
 */

#if defined(_MSC_VER)

  #define MYSQL_HARNESS_DISABLE_WARNINGS() \
    __pragma(warning(push)) \
    __pragma(warning(disable:))

  #define MYSQL_HARNESS_ENABLE_WARNINGS() __pragma(warning(pop))

#elif defined(__clang__) || \
      __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)

  #define MYSQL_HARNESS_PRAGMA_COMMON(cmd) _Pragma(#cmd)

  #ifdef __clang__
    #define MYSQL_HARNESS_PRAGMA(cmd) MYSQL_HARNESS_PRAGMA_COMMON(clang cmd)
  #elif __GNUC__
    #define MYSQL_HARNESS_PRAGMA(cmd) MYSQL_HARNESS_PRAGMA_COMMON(GCC cmd)
  #endif

  #define MYSQL_HARNESS_DISABLE_WARNINGS() \
    MYSQL_HARNESS_PRAGMA(diagnostic push) \
    MYSQL_HARNESS_PRAGMA(diagnostic ignored "-Wsign-conversion") \
    MYSQL_HARNESS_PRAGMA(diagnostic ignored "-Wpedantic") \
    MYSQL_HARNESS_PRAGMA(diagnostic ignored "-Wshadow") \
    MYSQL_HARNESS_PRAGMA(diagnostic ignored "-Wconversion") \
    MYSQL_HARNESS_PRAGMA(diagnostic ignored "-Wsign-compare") \
    MYSQL_HARNESS_PRAGMA(diagnostic ignored "-Wunused-parameter")

  #define MYSQL_HARNESS_ENABLE_WARNINGS() MYSQL_HARNESS_PRAGMA(diagnostic pop)

#else

  // Unsupported compiler, leaving warnings as they were.
  #define MYSQL_HARNESS_DISABLE_WARNINGS()
  #define MYSQL_HARNESS_ENABLE_WARNINGS()

#endif

#endif /* MYSQL_HARNESS_COMMON_INCLUDED */
