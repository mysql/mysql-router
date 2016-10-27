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

#ifndef MYSQL_HARNESS_KEYRING_MANAGER_INCLUDED
#define MYSQL_HARNESS_KEYRING_MANAGER_INCLUDED

#include <string>
#include "keyring.h"

namespace mysql_harness {

static const int kMaxKeyringKeyLength = 255;

/**
 * Initialize an instance of a keyring to be used in the application
 * from the contents of a file, using the given master key file.
 *
 * @param keyring_file_path path to the file where keyring is stored
 * @param master_key_path path to the file keyring master keys are stored
 * @param create_if_needed creates the keyring if it doesn't exist yet
 *
 * @return false if the keyring had to be created
 */
HARNESS_EXPORT bool init_keyring(const std::string &keyring_file_path,
                                 const std::string &master_key_path,
                                 bool create_if_needed);

/**
 * Initialize an instance of a keyring to be used in the application
 * from the contents of a file, using the given master key.
 *
 * @param keyring_file_path path to the file where keyring is stored
 * @param master_key master key for the keyring
 * @param create_if_needed creates the keyring if it doesn't exist yet
 *
 * @return false if the keyring had to be created
 */
HARNESS_EXPORT bool init_keyring_with_key(const std::string &keyring_file_path,
                                          const std::string &master_key,
                                          bool create_if_needed);

/**
 * Saves the keyring contents to disk.
 */
HARNESS_EXPORT void flush_keyring();


/**
 * Gets a previously initialized singleton instance of the keyring
 */
HARNESS_EXPORT Keyring *get_keyring();

/**
 * Clears the keyring singleton.
 */
HARNESS_EXPORT void reset_keyring();
}

#endif
