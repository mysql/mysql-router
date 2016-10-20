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

#ifndef MYSQLROUTER_UTILS_INCLUDED
#define MYSQLROUTER_UTILS_INCLUDED

#include <cstdarg>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#ifndef _WIN32
#  include <netdb.h>
#endif

namespace mysqlrouter {

// Some (older) compiler have no std::to_string available
template<typename T>
std::string to_string(const T &data) {
  std::ostringstream os;
  os << data;
  return os.str();
}

/** @brief Returns string formatted using given data
*
* Returns string formatted using given data accepting the same arguments
* and format specifies as the typical printf.
*
* @param format specify how to format the data
* @param ... variable argument list containing the data
* @returns formatted text as string
*/
std::string string_format(const char *format, ...);

/**
 * Split host and port
 *
 * @param data a string with hostname and port
 * @return std::pair<string, uint16_t> containing address and port
 */
std::pair<std::string, uint16_t> split_addr_port(const std::string data);

/**
 * Validates a string containing a TCP port
 *
 * Validates whether the data can be used as a TCP port. A TCP port is
 * a valid number in the range of 0 and 65535. The returned integer is
 * of type uint16_t.
 *
 * An empty data string will result in TCP port 0 to be returned.
 *
 * Throws runtime_error when the given string can not be converted
 * to an integer or when the integer is to big.
 *
 * @param data string containing the TCP port number
 * @return uint16_t the TCP port number
 */
uint16_t get_tcp_port(const std::string& data);

/** @brief Splits a string using a delimiter
 *
 * Splits a string using the given delimiter. When allow_empty
 * is true (default), tokens can be empty, and will be included
 * as empty in the result.
 *
 * @param data a string to split
 * @param delimiter a char used as delimiter
 * @param bool whether to allow empty tokens or not (default true)
 * @return std::vector<string> containing tokens
 */
std::vector<std::string> split_string(const std::string& data, const char delimiter, bool allow_empty = true);

/**
 * Removes leading whitespaces from the string
 *
 * @param str the string to be trimmed
 */
void left_trim(std::string& str);

/**
 * Removes trailing whitespaces from the string
 *
 * @param str the string to be trimmed
 */
void right_trim(std::string& str);

/**
 * Removes both leading and trailing whitespaces from the string
 *
 * @param str the string to be trimmed
 */
void trim(std::string& str);

/** @brief Dumps buffer as hex values
 *
 * Debugging function which dumps the given buffer as hex values
 * in rows of 16 bytes. When literals is true, characters in a-z
 * or A-Z, are printed as-is.
 *
 * @param buffer char array or front of vector<uint8_t>
 * @param count number of bytes to dump
 * @param start from where to start dumping
 * @param literals whether to show a-zA-Z as-is
 * @return string containing the dump
 */
std::string hexdump(const unsigned char *buffer, size_t count, long start = 0, bool literals = false);

/** @brief Returns the platform specific error code of last operation
 * Using errno in UNIX & Linux systems and GetLastError in Windows systems.
 * If myerrnum arg is not zero will use GetLastError in Windows (if myerrnum is zero in Unix will read *current* the errno).
 * @return the error code
 */
std::string get_last_error(int myerrnum = 0);

/** @brief Prompts for a password from the console.
 */
std::string prompt_password(const std::string &prompt);

/** @brief Override default prompt password function
 */
void set_prompt_password(const std::function<std::string (const std::string &)> &f);

#ifdef _WIN32
/** @brief Returns whether if the router process is running as a Windows Service
 */
bool is_running_as_service();
#endif

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
std::vector<std::string> wrap_string(const std::string &str, size_t width, size_t indent);

bool my_check_access(const std::string& path);

int mkdir(const std::string& dir, int mode);

int rmdir(const std::string& dir);

int delete_file(const std::string& path);

} // namespace mysqlrouter

#endif // MYSQLROUTER_UTILS_INCLUDED
