/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SRC_ROUTER_INCLUDE_MYSQLROUTER_WINDOWS_PASSWORD_VAULT_H_
#define SRC_ROUTER_INCLUDE_MYSQLROUTER_WINDOWS_PASSWORD_VAULT_H_

#include <map>
#include <string>



class PasswordVault {
 public:
  /** @brief Create an instance of the vault.
   *
   * On creation the vault cache is initialized with the contents of the vault
   * file at %APPDATA%/MySQL/MySQL Router/mysql_router_user_data.dat.
   * The passwords are stored in the cache in cleartext.
   */
  PasswordVault();

  /** @brief wipes the contents of the vault cache.
   */
  ~PasswordVault();

  /** @brief Updates a pair (section name, password) in the vault cache.
   *
   * If the record for the given section name & password does not exits, it is
   * created. If it exists it is just updated with the new password.
   *
   * @param section_name The name of the configuration section to store in the
   *   vault.
   * @param password The password, in clear text, of the user in the
   *   configuration section to store in the vault.
   */
  void update_password(const std::string& section_name, const std::string& password);

  /** @brief Retrieves the password, in clear text, for the given section as
   *     is stored in the vault.
   *
   * @param section_name The name of the configuration section for which to 
   *   retrieve the password.
   * @param out_password Output paramter. The password in clear text if the 
   *   section name was found in the vault.
   * @return true if a password was retrieved for the given section, false if 
   *   the section name could not be found in the vault.
   */
  bool get_password(const std::string& section_name, std::string& out_password) const;

  /** @brief Removes the password from the vault for the given section name.
   * 
   * After executing this method for a fiven section name, the method 
   *   get_password will return false for the same section name.
   *
   * @param section_name The name of the configuration section for which to 
   *   remove the password.
   */
  void remove_password(const std::string& section_name);

  /** @brief Stores the vault cache into persistent storage in encrypted form.
   *
   * The vault location in persistent storage is 
   * %APPDATA%/MySQL/MySQL Router/mysql_router_user_data.dat.
   */
  void store_passwords();

  /** @brief Wipes the contents of the vault file.
   * 
   * NOTE: The delete the vault cache (in memory) created for an instance of 
   * PasswordVault is done automatically in the destructor.
   */
  void clear_passwords();

 private:
  void load_passwords();
  std::string get_vault_path() const;
  // Password cache as pairs <section_name, password>
  std::map<std::string, std::string> _passwords;
  void password_scrambler(std::string& pass);
};

#endif  // SRC_ROUTER_INCLUDE_MYSQLROUTER_WINDOWS_PASSWORD_VAULT_H_
