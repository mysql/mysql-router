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

#include "keyring/keyring_file.h"
#include "common.h"
#include <fstream>
#include <memory>

#ifdef _WIN32
#include <aclapi.h>
#else
#include <sys/stat.h>
#endif


#ifdef _WIN32

// Smart pointers for WinAPI structures that use C-style memory management.
using SecurityDescriptorPtr =
    std::unique_ptr<SECURITY_DESCRIPTOR,
                    mysql_harness::StdFreeDeleter<SECURITY_DESCRIPTOR>>;
using SidPtr = std::unique_ptr<SID, mysql_harness::StdFreeDeleter<SID>>;

/**
 * Retrieves file's DACL security descriptor.
 *
 * @param[in] file_name File name.
 *
 * @return File's DACL security descriptor.
 *
 * @except std::exception Failed to retrieve security descriptor.
 */
static SecurityDescriptorPtr get_security_descriptor(
    const std::string& file_name) {
  static constexpr SECURITY_INFORMATION kReqInfo = DACL_SECURITY_INFORMATION;

  // Get the size of the descriptor.
  DWORD sec_desc_size;

  if (GetFileSecurityA(file_name.c_str(), kReqInfo, nullptr,
                       0, &sec_desc_size) == FALSE) {
    auto error = GetLastError();

    // We expect to receive `ERROR_INSUFFICIENT_BUFFER`.
    if (error != ERROR_INSUFFICIENT_BUFFER) {
      throw std::runtime_error("GetFileSecurity() failed (" + file_name +
                               "): " + std::to_string(error));
    }
  }

  SecurityDescriptorPtr sec_desc(
      static_cast<SECURITY_DESCRIPTOR*>(std::malloc(sec_desc_size)));

  if (GetFileSecurityA(file_name.c_str(), kReqInfo, sec_desc.get(),
                       sec_desc_size, &sec_desc_size) == FALSE) {
    throw std::runtime_error("GetFileSecurity() failed (" + file_name + "): " +
                             std::to_string(GetLastError()));
  }

  return sec_desc;
}

/**
 * Verifies permissions of an access ACE entry.
 *
 * @param[in] access_ace Access ACE entry.
 *
 * @except std::exception Everyone has access to the ACE access entry or
 *                        an error occured.
 */
static void check_ace_access_rights(ACCESS_ALLOWED_ACE* access_ace) {
  SID* sid = reinterpret_cast<SID*>(&access_ace->SidStart);
  DWORD sid_size = SECURITY_MAX_SID_SIZE;
  SidPtr everyone_sid(static_cast<SID*>(std::malloc(sid_size)));

  if (CreateWellKnownSid(WinWorldSid, nullptr,
                         everyone_sid.get(), &sid_size) == FALSE) {
    throw std::runtime_error("CreateWellKnownSid() failed: " +
                             std::to_string(GetLastError()));
  }

  if (EqualSid(sid, everyone_sid.get())) {
    if (access_ace->Mask & (FILE_EXECUTE)) {
      throw std::runtime_error("Invalid keyring file access rights "
                               "(Execute privilege granted to Everyone).");
    }
    if (access_ace->Mask &
        (FILE_WRITE_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES)) {
      throw std::runtime_error("Invalid keyring file access rights "
                               "(Write privilege granted to Everyone).");
    }
    if (access_ace->Mask &
        (FILE_READ_DATA | FILE_READ_EA | FILE_READ_ATTRIBUTES)) {
      throw std::runtime_error("Invalid keyring file access rights "
                               "(Read privilege granted to Everyone).");
    }
  }
}

/**
 * Verifies access permissions in a DACL.
 *
 * @param[in] dacl DACL to be verified.
 *
 * @except std::exception DACL contains an ACL entry that grants full access to
 *                        Everyone or an error occured.
 */
static void check_acl_access_rights(ACL* dacl) {
  ACL_SIZE_INFORMATION dacl_size_info;

  if (GetAclInformation(dacl, &dacl_size_info, sizeof(dacl_size_info),
                        AclSizeInformation) == FALSE) {
    throw std::runtime_error("GetAclInformation() failed: " +
                             std::to_string(GetLastError()));
  }

  for (DWORD ace_idx = 0; ace_idx < dacl_size_info.AceCount; ++ace_idx) {
    LPVOID ace = nullptr;

    if (GetAce(dacl, ace_idx, &ace) == FALSE) {
      throw std::runtime_error("GetAce() failed: " +
                               std::to_string(GetLastError()));
      continue;
    }

    if (static_cast<ACE_HEADER*>(ace)->AceType == ACCESS_ALLOWED_ACE_TYPE)
      check_ace_access_rights(static_cast<ACCESS_ALLOWED_ACE*>(ace));
  }
}

/**
 * Verifies access permissions in a security descriptor.
 *
 * @param[in] sec_desc Security descriptor to be verified.
 *
 * @except std::exception Security descriptor grants full access to
 *                        Everyone or an error occured.
 */
static void check_security_descriptor_access_rights(
    SecurityDescriptorPtr sec_desc) {
  BOOL dacl_present;
  ACL* dacl;
  BOOL dacl_defaulted;

  if (GetSecurityDescriptorDacl(sec_desc.get(), &dacl_present, &dacl,
                                &dacl_defaulted) == FALSE) {
    throw std::runtime_error("GetSecurityDescriptorDacl() failed: " +
                             std::to_string(GetLastError()));
  }

  if (!dacl_present) {
    // No DACL means: no access allowed. Which is fine.
    return;
  }

  if (!dacl) {
    // Empty DACL means: all access allowed.
      throw std::runtime_error("Invalid keyring file access rights "
                               "(Everyone has full access rights).");
  }

  check_acl_access_rights(dacl);
}

#endif // _WIN32

/**
 * Verifies access permissions of a file.
 *
 * On Unix systems it throws if file's permissions differ from 600.
 * On Windows it throws if file can be accessed by Everyone group.
 *
 * @param[in] file_name File to be verified.
 *
 * @except std::exception File access rights are too permissive or
 *                        an error occured.
 */
static void check_file_access_rights(const std::string& file_name) {
#ifdef _WIN32

  check_security_descriptor_access_rights(get_security_descriptor(file_name));

#else

  struct stat status;

  if (stat(file_name.c_str(), &status) != 0) {
    throw std::runtime_error("stat() failed (" + file_name + "): " +
                             mysql_harness::get_strerror(errno));
  }

  static constexpr mode_t kFullAccessMask = (S_IRWXU | S_IRWXG | S_IRWXO);
  static constexpr mode_t kRequiredAccessMask = (S_IRUSR | S_IWUSR);

  if ((status.st_mode & kFullAccessMask) != kRequiredAccessMask)
    throw std::runtime_error("Invalid keyring file access rights.");

#endif // _WIN32
}


namespace mysql_harness {


void KeyringFile::save(const std::string& file_name,
                       const std::string& key) const {
  // Serialize keyring.
  auto buffer = serialize(key);

  // Save keyring data to file.
  std::ofstream file;

  file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
  try {
    file.open(file_name,
              std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    file.write(buffer.data(), buffer.size());
    file.close();
    make_file_private(file_name);
  } catch (std::exception& e) {
    throw std::runtime_error(std::string("Failed to save keyring file: ") +
                             e.what());
  }
}

void KeyringFile::load(const std::string& file_name, const std::string& key) {
  // Verify keyring file's access permissions.
  check_file_access_rights(file_name);

  // Read keyring data from file.
  std::ifstream file;
  std::vector<char> buffer;

  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(file_name,
              std::ifstream::in | std::ifstream::binary | std::ifstream::ate);

    auto data_size = static_cast<std::size_t>(file.tellg());

    buffer.resize(data_size);
    file.seekg(0);
    file.read(buffer.data(), data_size);
  } catch (std::exception& e) {
    throw std::runtime_error(std::string("Failed to load keyring file: ") +
                             e.what());
  }

  // Parse keyring data.
  parse(key, buffer.data(), buffer.size());
}


} // namespace mysql_harness
