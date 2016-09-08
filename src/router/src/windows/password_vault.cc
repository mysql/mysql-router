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

#include "mysqlrouter/windows/password_vault.h"

#include <windows.h>
#include <shlobj.h>
#include <comdef.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <strstream>
#include <sstream>
#include <vector>

#include "mysqlrouter/utils.h"

/*
* Attempts to create directory if doesn't exists, otherwise just returns.
* If there is an error, an exception is thrown.
*/
static void ensure_dir_exists(const std::string& path) {
  const char *dir_path = path.c_str();
  DWORD dwAttrib = GetFileAttributesA(dir_path);

  if (dwAttrib != INVALID_FILE_ATTRIBUTES) {
    return;
  } else if (!CreateDirectoryA(dir_path, NULL)) {
    throw std::runtime_error(mysqlrouter::string_format(
      "Error when creating directory %s with error: %s", dir_path,
      GetLastError()));
  }
}


static std::string get_user_config_path() {
  std::string path_separator;
  std::string path;
  std::vector <std::string> to_append;

  path_separator = "\\";
  char szPath[MAX_PATH];
  HRESULT hr;

  if (SUCCEEDED(hr = SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, szPath))) {
    path.assign(szPath);
  } else {
    _com_error err(hr);
    throw std::runtime_error(mysqlrouter::string_format(
      "Error when gathering the APPDATA folder path: %s", err.ErrorMessage()));
  }

  to_append.push_back("MySQL");
  to_append.push_back("MySQL Router");

  // Up to know the path must exist since it was retrieved from OS standard
  // neabs we need to guarantee the rest of the path exists
  if (!path.empty()) {
    for (size_t index = 0; index < to_append.size(); index++) {
      path += path_separator + to_append[index];
      ensure_dir_exists(path);
    }

    path += path_separator;
  }

  return path;
}


PasswordVault::PasswordVault() {
  load_passwords();
}

PasswordVault::~PasswordVault() {
  // scrambles all the passwords first
  for (auto &it : _passwords) {
  // for (std::map<std::string, std::string>::iterator it = _passwords.begin();
  // it != _passwords.end(); ++it) {
    std::string& pass = it.second;
    password_scrambler(pass);
  }
  _passwords.clear();
}

std::string PasswordVault::get_vault_path() const {
  return get_user_config_path() + "mysql_router_user_data.dat";
}

void PasswordVault::password_scrambler(std::string& pass) {
  for (size_t i = 0; i < pass.length(); ++i)
    pass.at(i) = '*';
}

void PasswordVault::remove_password(const std::string& section_name) {
  _passwords.erase(section_name);
}

void PasswordVault::update_password(const std::string& section_name,
  const std::string& password) {
  _passwords[section_name] = password;
}

bool PasswordVault::get_password(const std::string& section_name,
  std::string& out_password) const {
  std::map<std::string, std::string>::const_iterator it =
    _passwords.find(section_name);
  if (it != _passwords.end()) {
    out_password = it->second;
    return true;
  } else {
    return false;
  }
}

void PasswordVault::clear_passwords() {
  const std::string vault_path = get_vault_path();
  std::ofstream f(vault_path, std::ios_base::trunc);
  f.flush();
  for (auto &it : _passwords) {
    std::string& pass = it.second;
    password_scrambler(pass);
  }

  _passwords.clear();
}

void PasswordVault::load_passwords() {
  const std::string vault_path = get_vault_path();
  std::unique_ptr<std::ifstream> file_vault(new std::ifstream(vault_path, std::ios_base::binary));
  if (!file_vault) {
    // creates the file if doesn't exist
    std::ofstream file_vault_out(vault_path, std::ios_base::binary);
    if (!file_vault_out)
      throw std::runtime_error("Cannot open the vault at '" + vault_path +"'");
    file_vault_out.close();
    file_vault.reset(new std::ifstream(vault_path, std::ios_base::binary));
  }
  std::streampos begin = file_vault->tellg();
  file_vault->seekg(0, std::ios::end);
  std::streampos end = file_vault->tellg();
  if (end - begin == 0) return;
  std::unique_ptr<char, void(*)(char*)> buf(new char[end - begin], 
    [](char* ptr) { delete[] ptr; });
  file_vault->seekg(0, std::ios::beg);
  file_vault->read(buf.get(), end - begin);

  // decrypt the data
  DATA_BLOB buf_encrypted;
  DATA_BLOB buf_decrypted;
  buf_encrypted.pbData = reinterpret_cast<BYTE *>(buf.get());
  buf_encrypted.cbData = end - begin;
  if (!CryptUnprotectData(&buf_encrypted, NULL, NULL, NULL, NULL, 0,
    &buf_decrypted)) {
    DWORD code = GetLastError();
    throw std::runtime_error(mysqlrouter::string_format(
      "Error when decrypting the vault at '%s' with code '%d'", vault_path,
      code));
  }

  std::strstream ss(reinterpret_cast<char *>(buf_decrypted.pbData),
    buf_decrypted.cbData, std::ios_base::in);

  std::string line;
  std::string section_name;
  int i = 0;
  while (std::getline(ss, line)) {
    if (i % 2 == 0)
      section_name = line;
    else
      _passwords[section_name] = line;
    i++;
  }
  LocalFree(buf_decrypted.pbData);
}

void PasswordVault::store_passwords() {
  std::stringstream ss(std::ios_base::out);
  for (auto &it : _passwords) {
  // for (std::map<std::string, std::string>::iterator it = _passwords.begin();
    // it != _passwords.end(); ++it)
    ss << it.first << std::endl;
    ss << it.second << std::endl;
  }
  ss.flush();
  // encrypt the data
  DATA_BLOB buf_decrypted;
  DATA_BLOB buf_encrypted;
  std::string data = ss.str();
  buf_decrypted.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(data.c_str()));
  buf_decrypted.cbData = ss.str().size();
  if (!CryptProtectData(&buf_decrypted, NULL, NULL, NULL, NULL, 0, &buf_encrypted)) {
    DWORD code = GetLastError();
    throw std::runtime_error(mysqlrouter::string_format(
      "Error when encrypting the vault with code '%d'", code));
  }

  const std::string vault_path = get_vault_path();
  std::ofstream f(vault_path, std::ios_base::trunc | std::ios_base::binary);
  if (!f)
    throw std::runtime_error("Cannot open the vault at '" + vault_path + "'");
  f.write(reinterpret_cast<char *>(buf_encrypted.pbData), buf_encrypted.cbData);
  f.flush();
  LocalFree(buf_encrypted.pbData);
}
