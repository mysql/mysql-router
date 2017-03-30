/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MAGIC_INCLUDED
#define MAGIC_INCLUDED

#include <stdexcept>

class bad_suki : public std::runtime_error {
 public:
  explicit bad_suki(const std::string& msg) : std::runtime_error(msg) {}
};

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

#endif /* MAGIC_INCLUDED */
