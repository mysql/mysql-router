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

#ifndef FABRIC_CACHE_UTILS_INCLUDED
#define FABRIC_CACHE_UTILS_INCLUDED

#include <string>
#include <time.h>

using std::string;

/**
 * Return a string representation of the input character string.
 *
 * @param input_str A character string.
 *
 * @return A string object encapsulation of the input character string. An empty
 *         string if input string is nullptr.
 */
string get_string(const char *input_str);

/** @class ValueComparator
 * Base utility class for comparing two values. The compare method will be
 * overwritten by the base classes for converting the input strings into
 * appropriate datatypes for comparison.
 */
class ValueComparator {
public:
  virtual int compare(string val_a, string val_b) = 0;
};

/** @class IntegerValueComparator
 * Convert the input strings into integers and compare
 */
class IntegerValueComparator final : public ValueComparator {
public:
  /** @brief Compares two integer values
   *
   * Compare two integer values.
   */
  int compare(string val_a, string val_b);
};

/** @class DateTimeValueComparator
 * Compare the input values into datetime values and compare.
 */
class DateTimeValueComparator final : public ValueComparator {
public:
  /** @brief Converts the datetime string to a time_t value
   *
   * @param datetime_str The string that needs to be converted
   *
   * @return The converted time_t value.
   */
  time_t convert_to_time_t(string datetime_str);

  /** @brief Compares two strings containing DATETIME
   *
   * Compares the two strings containing values in DATETIME format.
   *
   * @param val_a The first DATETIME value.
   * @param val_b The second DATETIME value.
   *
   * @return 1 if val_a > val_b
   *        -1 if val_a < val_b
   *         0 if val_a = val_b
   */
  int compare(string val_a, string val_b);
};

/** @class StringValueComparator
 * Compare the input strings as string values.
 */
class StringValueComparator final : public ValueComparator {
public:
  /** @brief Compares two strings
   *
   * Compare the two input strings, using standard string comparison.
   *
   * @param val_a The first string.
   * @param val_b The second string.
   *
   * @return 1 if val_a > val_b
   *        -1 if val_a < val_b
   *         0 if val_a = val_b.
   */
  int compare(string val_a, string val_b);
};

/** @class MD5HashValueComparator
 * Compare the input MD5 hash values.
 */
class MD5HashValueComparator final : public ValueComparator {
public:
  /** @brief Converts hexadecimal character to integer
   * Convert a hexadecimal character to an integer.
   *
   * @param c The character that needs to be converted to an hexadecimal value.
   */
  int convert_hexa_char_to_int(char c);

  /** @brief Compares two strings containing a MD5 hash
   *
   * Compares two strings containing a MD5 hash.
   *
   * @param val_a The first MD5 hash string.
   * @param val_b The second MD5 hash string.
   *
   * @return 1 if val_a > val_b
   *        -1 if val_a < val_b
   *         0 if val_a = val_b.
   */
  int compare(string val_a, string val_b);
};

#endif // FABRIC_CACHE_UTILS_INCLUDED
