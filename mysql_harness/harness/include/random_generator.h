/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_HARNESS_RANDOM_GENERATOR_INCLUDED
#define MYSQL_HARNESS_RANDOM_GENERATOR_INCLUDED

#include <string>

#include "harness_export.h"

namespace mysql_harness {

class HARNESS_EXPORT RandomGeneratorInterface {
 public:
  enum AlphabetContent: unsigned {
    AlphabetDigits = 0x1,
    AlphabetLowercase = 0x2,
    AlphabetUppercase = 0x4,
    AlphabetSpecial = 0x8,
    AlphabetAll = 0xFF
  };

  /** @brief Generates a random string out of selected alphabet
   *
   * @param length length of string requested
   * @param alphabet_mask bitmasmask indicating which alphabet symbol groups should be
   *                      used for indentifier generation (see AlphabetContent enum
   *                      for possible values that can be or-ed)
   * @return string with the generated random chars
   *
   * @throws std::invalid_argument when the alphabet_mask is empty or invalid
   *
   */
  virtual std::string generate_identifier(unsigned length, unsigned alphabet_mask = AlphabetAll) = 0;

  /** @brief Generates a random password that adheres to the STRONG password requirements:
   *         * contains at least 1 digit
   *         * contains at least 1 uppercase letter
   *         * contains at least 1 lowercase letter
   *         * contains at least 1 special character
   *
   * @param length length of requested password (should be at least 8)
   * @return string with the generated password
   *
   * @throws std::invalid_argument when the requested length is less than 8
   *
   */
  virtual std::string generate_strong_password(unsigned length) = 0;
};

class HARNESS_EXPORT RandomGenerator : public RandomGeneratorInterface {
 public:
  std::string generate_identifier(unsigned length, unsigned alphabet_mask = AlphabetAll) override;
  std::string generate_strong_password(unsigned length) override;
};

class HARNESS_EXPORT FakeRandomGenerator : public RandomGeneratorInterface {
 public:
  // returns "012345678901234567890123...", truncated to password_length
  std::string generate_identifier(unsigned length, unsigned) override;
  // returns "012345678901234567890123...", truncated to password_length
  std::string generate_strong_password(unsigned length) override;
};

} // namespace mysql_harness


#endif // MYSQL_HARNESS_RANDOM_GENERATOR_INCLUDED
