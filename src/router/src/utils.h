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

#ifndef ROUTER_UTILS_INCLUDED
#define ROUTER_UTILS_INCLUDED

/** @file
 * @brief MySQL Router utility functions
 *
 * This file defines global or free functions as well as exception classes
 * used by the MySQL Router application. The namespace used is `mysqlrouter`.
 *
 */

#include <string>
#include <vector>

using std::vector;
using std::string;

/**
 * @brief MySQL Router utilities and exception classes
 */
namespace mysqlrouter {

/** @brief Substitutes placeholders of environment variables in a string
 *
 * Substitutes placeholders of environement variables in a string. A
 * placeholder contains the name of the variable and will be fetched
 * from the environment. The substitution is done in-place.
 *
 * Note that it is not an error to pass a string with no variable to
 * be substituted - in such case success will be returned, and the
 * original string will remain unchanged.
 * Also note, that if an error occurs, the resulting string value is
 * undefined (it will be left in an inconsistent state).
 *
 * @return bool (success flag)
 */
bool substitute_envvar(std::string &line) noexcept;

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
