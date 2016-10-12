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

#include "common.h"
#include <sstream>
#include <memory>
#include <string.h>

#ifdef _WIN32
#include <aclapi.h>
#else
#include <sys/stat.h>
#endif


#ifdef _WIN32

/**
 * Deleter for smart pointers pointing to objects that require to be released
 * with `LocalFree`.
 */
class LocalFreeDeleter {
public:
  void operator()(HLOCAL hnd) {
    LocalFree(hnd);
  }
};

/**
 * Sets file permissions for Everyone group.
 *
 * @param[in] file_name File name.
 * @param[in] mask Access rights mask for Everyone group.
 *
 * @except std::exception Failed to change file permissions.
 */
static void set_everyone_group_access_rights(const std::string& file_name,
                                             DWORD mask) {
  using SecurityDescriptorPtr =
    std::unique_ptr<SECURITY_DESCRIPTOR, LocalFreeDeleter>;
  using AclPtr = std::unique_ptr<ACL, LocalFreeDeleter>;
  using SidPtr = std::unique_ptr<SID, mysql_harness::StdFreeDeleter<SID>>;

  // Create Everyone SID.
  DWORD sid_size = SECURITY_MAX_SID_SIZE;
  SidPtr everyone_sid(static_cast<SID*>(std::malloc(sid_size)));

  if (CreateWellKnownSid(WinWorldSid, NULL, everyone_sid.get(), &sid_size) ==
        FALSE) {
    throw std::runtime_error("CreateWellKnownSid() failed: " +
                             std::to_string(GetLastError()));
  }

  // Get file security descriptor.
  ACL* old_dacl;
  SecurityDescriptorPtr sec_desc;

  {
    PSECURITY_DESCRIPTOR sec_desc_tmp;
    auto result = GetNamedSecurityInfoA(file_name.c_str(), SE_FILE_OBJECT,
                                        DACL_SECURITY_INFORMATION, NULL, NULL,
                                        &old_dacl, NULL, &sec_desc_tmp);

    if (result != ERROR_SUCCESS) {
      throw std::runtime_error("GetNamedSecurityInfo() failed: " +
                               std::to_string(result));
    }

    // If everything went fine, we move raw pointer to smart pointer.
    sec_desc.reset(reinterpret_cast<SECURITY_DESCRIPTOR*>(sec_desc_tmp));
  }

  // Setting access permissions for Everyone group.
  EXPLICIT_ACCESSA ea[1];

  memset(&ea, 0, sizeof(ea));
  ea[0].grfAccessPermissions = mask;
  ea[0].grfAccessMode = SET_ACCESS;
  ea[0].grfInheritance = NO_INHERITANCE;
  ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea[0].Trustee.ptstrName = reinterpret_cast<char*>(everyone_sid.get());

  // Create new ACL permission set.
  AclPtr new_dacl;

  {
    ACL* new_dacl_tmp;
    auto result = SetEntriesInAclA(1, &ea[0], old_dacl, &new_dacl_tmp);

    if (result != ERROR_SUCCESS) {
      throw std::runtime_error("SetEntriesInAcl() failed: " +
                               std::to_string(result));
    }

    // If everything went fine, we move raw pointer to smart pointer.
    new_dacl.reset(new_dacl_tmp);
  }

  // Set file security descriptor.
  auto result = SetNamedSecurityInfoA(const_cast<char*>(file_name.c_str()),
                                 SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                 NULL, NULL, new_dacl.get(), NULL);

  if (result != ERROR_SUCCESS) {
    throw std::runtime_error("SetNamedSecurityInfo() failed: " +
                             std::to_string(result));
  }
}

#else

/**
 * Sets access permissions for a file.
 *
 * @param[in] file_name File name.
 * @param[in] mask Access permission mask.
 *
 * @except std::exception Failed to change file permissions.
 */
static void throwing_chmod(const std::string& file_name, mode_t mask) {
  if (chmod(file_name.c_str(), mask) != 0) {
    throw std::runtime_error("chmod() failed: " + file_name + ": " +
                             mysql_harness::get_strerror(errno));
  }
}

#endif // _WIN32


namespace mysql_harness {

void make_file_public(const std::string& file_name) {
#ifdef _WIN32
  set_everyone_group_access_rights(
    file_name, FILE_GENERIC_EXECUTE | FILE_GENERIC_WRITE | FILE_GENERIC_READ);
#else
  throwing_chmod(file_name, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
}

void make_file_private(const std::string& file_name) {
#ifdef _WIN32
  set_everyone_group_access_rights(file_name, 0);
#else
  throwing_chmod(file_name, S_IRUSR | S_IWUSR);
#endif
}

std::string get_strerror(int err) {
    char msg[256];
    std::string result;

#if  !_GNU_SOURCE && (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600)
  int ret = strerror_r(err, msg, sizeof(msg));
  if (ret) {
    return "errno= " + std::to_string(err) + " (strerror_r failed: " + std::to_string(ret) + ")";
  } else {
    result = std::string(msg);
  }
#elif defined(_WIN32)
  int ret = strerror_s(msg, sizeof(msg), err);
  if (ret) {
    return "errno= " + std::to_string(err) + " (strerror_s failed: " + std::to_string(ret) + ")";
  } else {
    result = std::string(msg);
  }
#else // GNU version
  char* ret = strerror_r(err, msg, sizeof(msg));
  result = std::string(ret);
#endif

  return result;
}

}
