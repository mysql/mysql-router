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

#ifndef MYSQL_HARNESS_KEYRING_FILE_INCLUDED
#define MYSQL_HARNESS_KEYRING_FILE_INCLUDED

#include "keyring_memory.h"


namespace mysql_harness {


/**
 * KeyringFile class.
 *
 * Implements Keyring interface and provides additional methods for loading and
 * saving keyring to file.
 */
class HARNESS_EXPORT KeyringFile : public KeyringMemory {
 public:
  KeyringFile() = default;

  /**
   * Saves keyring to file.
   *
   * @param[in] file_name Keyring file name.
   * @param[in] key Key used for encryption.
   *
   * @except std::exception Saving to file failed.
   */
  void save(const std::string& file_name, const std::string& key) const;

  /**
   * Load keyring from file.
   *
   * @param[in] file_name Keyring file name.
   * @param[in] key Key used for decryption.
   *
   * @except std::exception Loading from file failed.
   */
  void load(const std::string& file_name, const std::string& key);
};


} // namespace mysql_harness


#endif // MYSQL_HARNESS_KEYRING_FILE_INCLUDED
