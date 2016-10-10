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

#ifndef MYSQL_HARNESS_KEYRING_MEMORY_INCLUDED
#define MYSQL_HARNESS_KEYRING_MEMORY_INCLUDED

#include "keyring.h"
#include <map>
#include <vector>


namespace mysql_harness {


/**
 * KeyringMemory class.
 *
 * Implements Keyring interface and provides additional methods for parsing
 * and serialization using a simple binary format. Also, handles AES encryption.
 * Used primarily for testing and as a base for KeyringFile.
 */
class HARNESS_EXPORT KeyringMemory : public Keyring {
 public:
  constexpr static unsigned int kFormatVersion = 0;

  KeyringMemory() = default;

  /**
   * Serializes and encrypts keyring data to memory buffer.
   *
   * @param[in] key Key used for encryption.
   *
   * @return Serialized keyring data.
   *
   * @thorw std::exception Serialization failed.
   */
  std::vector<char> serialize(const std::string& key) const;

  /**
   * Parses and decrypts keyring data.
   *
   * @param[in] key Key used for decryption.
   * @param[in] buffer Serialized keyring data.
   * @param[in] buffer_size Size of the data.
   *
   * @thorw std::exception Parsing failed.
   */
  void parse(const std::string& key, const char* buffer,
             std::size_t buffer_size);

  // Keyring interface.
  virtual void store(const std::string &uid,
                     const std::string &attribute,
                     const std::string &value) override;

  virtual std::string fetch(const std::string &uid,
                            const std::string &attribute) const override;

  virtual void remove(const std::string &uid) override;

  virtual void remove_attribute(const std::string &uid,
                                const std::string &attribute) override;

 private:
  std::map<std::string, std::map<std::string, std::string>> entries_;
};


} // namespace mysql_harness


#endif // MYSQL_HARNESS_KEYRING_MEMORY_INCLUDED
