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

#ifndef ROUTER_TESTS_TEST_HELPERS_INCLUDED
#define ROUTER_TESTS_TEST_HELPERS_INCLUDED

#include "filesystem.h"

/** @brief Returns the CMake source root folder
 *
 * @return mysql_harness::Path
 */
Path get_cmake_source_dir();

/** @brief Gets environment variable as path
 *
 * Gets environment envvar and returns it as Path. When the environment
 * variable is not set, the alternative is tried.
 *
 * Throws runtime_error when the folder is not available.
 *
 * @param envvar Name of the environment variable
 * @param Path Alternative Path when environment variable is not available
 * @return mysql_harness::Path
 */
Path get_envvar_path(const std::string &envvar, Path alternative);

/** @brief Returns the current working directory
 *
 * Uses `getcwd()` and returns the current working directory as as std::string.
 *
 * Throws std::runtime_error on errors.
 *
 * @return std::string
 */
const std::string get_cwd();

/** @brief Changes the current working directory
 *
 * Uses `chdir()` to change the current working directory. When succesfully
 * change to the folder, the old working directory is returned.
 *
 * Throws std::runtime_error on errors.
 *
 * @return std::string
 */
const std::string change_cwd(std::string &dir);

/** @brief Checks whether string ends with the specified suffix
 *
 * Returns true if the string ends with the given suffix.
 *
 * @return bool
 */
bool ends_with(const std::string &str, const std::string &suffix);

/** @brief Checks whether string starts with the specified prefix
 *
 * Returns true if the string begins with the given prefix.
 *
 * @return bool
 */
bool starts_with(const std::string &str, const std::string &prefix);

#endif // ROUTER_TESTS_TEST_HELPERS_INCLUDED
