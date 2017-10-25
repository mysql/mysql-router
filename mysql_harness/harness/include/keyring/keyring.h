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

#ifndef MYSQL_HARNESS_KEYRING_INCLUDED
#define MYSQL_HARNESS_KEYRING_INCLUDED

#include "harness_export.h"
#include <string>
#include <stdexcept>


namespace mysql_harness {


/**
 * Keyring interface.
 *
 * Keyrings are responsible for storage and retrieval of sensitive data
 * (such as login credentials).
 */
class HARNESS_EXPORT Keyring {
 public:
  virtual ~Keyring() = default;

  /**
   * Stores an attribute value in an entry.
   *
   * @param[in] uid Entry id.
   * @param[in] attribute Attribute id.
   * @param[in] value Attribute value.
   */
  virtual void store(const std::string &uid,
                     const std::string &attribute,
                     const std::string &value) = 0;

  /**
   * Retrieves attribute value from an entry.
   *
   * @param[in] uid Entry id.
   * @param[in] attribute Attribute id.
   *
   * @return Attribute value.
   *
   * @exception std::out_of_range Attribute not found.
   */
  virtual std::string fetch(const std::string &uid,
                            const std::string &attribute) const = 0;

  /**
   * Removes an entry.
   *
   * @param[in] uid Entry id.
   */
  virtual void remove(const std::string &uid) = 0;

  /**
   * Removes an attribute from an entry.
   *
   * @param[in] uid Entry id.
   * @param[in] attribute Attribute id.
   */
  virtual void remove_attribute(const std::string &uid,
                                const std::string &attribute) = 0;
};


} // namespace mysql_harness


#endif // MYSQL_HARNESS_KEYRING_INCLUDED
