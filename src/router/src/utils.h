/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTER_UTILS_INCLUDED
#define ROUTER_UTILS_INCLUDED

/** @file
 * @brief MySQL Router utility functions
 *
 * This file defines global or free functions as well as exception classes
 * used by the MySQL Router application. The namespace used is `mysqlrouter`.
 *
 */

#include <exception>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

using std::vector;
using std::string;

/**
 * @brief MySQL Router utilities and exception classes
 */
namespace mysqlrouter {

/** @brief Exception thrown for errors involving environment variables
 *
 * Exception thrown for errors involving environment variables.
 */
class envvar_error : public std::runtime_error {
public:
  explicit envvar_error(const string &what_arg) : std::runtime_error(what_arg) { }
};

/** @brief Exception thrown when there is no placeholder was found
 *
 * Exception thrown when there is no placeholder was found, meaning, there
 * is no `ENV{variable_name}` found.
 */
class envvar_no_placeholder : public envvar_error {
public:
  explicit envvar_no_placeholder(const string &what_arg) : envvar_error(what_arg) { }
};

/** @brief Exception thrown when environment placeholder is wrongly used
 *
 * Exception thrown when environment placeholder is wrongly used. For example,
 * when the curly braces are are not closed or no name was given:
 *
 *     ENV{HOME/bin
 *     /var/run/ENV{}/
 */
class envvar_bad_placeholder : public envvar_error {
public:
  explicit envvar_bad_placeholder(const string &what_arg) : envvar_error(what_arg) { }
};

/** @brief Exception thrown when variable in placeholder is not available
 *
 * Exception thrown when variable in placeholder is not available in the
 * environment.
 */
class envvar_not_available : public envvar_error {
public:
  explicit envvar_not_available(const string &what_arg) : envvar_error(what_arg) { }
};

/** @brief Substitutes placeholders of environment variables in a string
 *
 * Substitutes placeholders of environement variables in a string. A
 * placeholder contains the name of the variable and will be fetched
 * from the environment. If the variable is not available, an exception
 * is thrown.
 *
 * The substitution is done in-place.
 *
 * @throws envvar_no_placeholder when no placeholder was found in the string
 * @throws envvar_bad_placeholder when placeholder is wrongly formatted or
 * contains no name
 * @throws envvar_not_available when the variable is not available in the
 * environment
 */
void substitute_envvar(string &line);

/** @brief Wraps the given string
 *
 * Wraps the given string based on the spaces between words.
 * New lines are respected; carriage return and tab characters are
 * removed.
 *
 * The `width` specifies how much characters will in each line. It is also
 * possible to prefix each line with a number of spaces using the `indent_size` argument.
 *
 * @param str string to wrap
 * @param width maximum line length
 * @param indent number of spaces to prefix each line with
 * @return vector of strings
 */
vector<string> wrap_string(const string &str, size_t width, size_t indent);

} // namespace mysqlrouter

#endif // ROUTER_UTILS_INCLUDED
