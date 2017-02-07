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

#include "random_generator.h"

#include <assert.h> // <cassert> is flawed: assert() lands in global namespace on Ubuntu 14.04, not std::
#include <random>

namespace mysql_harness {
                                                                 // sizeof(alphabet)-1 ----vv
std::string RandomGenerator::generate_password(unsigned password_length, unsigned base /*= 87*/) noexcept /*override*/ {
  constexpr char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ~@#$^&*()-=+]}[{|;:.>,</?";
  assert(base <= sizeof(alphabet) - 1); // unsupported base requested (-1 for string terminator)
  assert(base > 1);                     // sanity check

  std::random_device rd;
  std::string pwd;
  std::uniform_int_distribution<unsigned long> dist(0, base - 1);

  for (unsigned i = 0; i < password_length; i++)
    pwd += alphabet[dist(rd)];

  return pwd;
}

// returns "012345678901234567890123...", truncated to password_length
std::string FakeRandomGenerator::generate_password(unsigned password_length, unsigned) noexcept /*override*/ {
  std::string pwd;
  for (unsigned i = 0; i < password_length; i++)
    pwd += static_cast<char>('0' + i % 10);
  return pwd;
}

} // namespace mysql_harness
