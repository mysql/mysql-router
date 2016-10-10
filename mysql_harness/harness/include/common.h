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
 * Default callback for `StdFreeDeleter`.
 */
template<typename T>
class DefaultStdFreeDeleterCallback {
public:
 void operator()(T* ptr) {
 }
};

/**
 * Deleter for smart pointers pointing to objects allocated with `std::malloc`.
 */
template<typename T, typename Callback = DefaultStdFreeDeleterCallback<T>>
class StdFreeDeleter {
public:
 void operator()(T* ptr) {
   Callback callback;

   callback(ptr);
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

}

#endif /* MYSQL_HARNESS_COMMON_INCLUDED */
