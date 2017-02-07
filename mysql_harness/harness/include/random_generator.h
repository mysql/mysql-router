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
  /** @brief Generates a random (password) string
   *
   * @param password_length length of string requested
   * @param base            number of possible random values per character
   * @return string with random chars (aka password string)
   *
   */       // sizeof(alphabet)-1 inside RandomGenerator::generate_password() ----vv
  virtual std::string generate_password(unsigned password_length, unsigned base = 87) noexcept = 0;
};

class HARNESS_EXPORT RandomGenerator : public RandomGeneratorInterface {
 public:                                        // sizeof(alphabet)-1 ----vv
  std::string generate_password(unsigned password_length, unsigned base = 87) noexcept override;
};

class HARNESS_EXPORT FakeRandomGenerator : public RandomGeneratorInterface {
 public:
  // returns "012345678901234567890123...", truncated to password_length
  std::string generate_password(unsigned password_length, unsigned) noexcept override;
};

} // namespace mysql_harness


#endif // MYSQL_HARNESS_RANDOM_GENERATOR_INCLUDED
