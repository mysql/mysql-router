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

// smart pointer for SID, allocated with LocalAlloc.
typedef std::unique_ptr<SID, LocalFreeDeleter> SidPtr;

/**
 * Gets the SID of the current process user.
 * The SID in Windows is the Security IDentifier, a security principal to which 
 * permissions are attached (machine, user group, user).
 */
static void GetCurrentUserSid(SidPtr& pSID) {
  typedef std::unique_ptr<TOKEN_USER, LocalFreeDeleter> TokenUserPtr;
  DWORD i, dw_size = 0;
  HANDLE h_token;
  TOKEN_INFORMATION_CLASS token_class = TokenUser;
  // Gets security token of the current process
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_READ | TOKEN_QUERY, &h_token)) {
    throw std::runtime_error("OpenProcessToken() failed: " + std::to_string(GetLastError()));
  }
  // Gets the user token from the security token (this one only finds out the buffer size required)
  if (!GetTokenInformation(h_token, token_class, NULL, 0, &dw_size)) {
    DWORD dwResult = GetLastError();
    if (dwResult != ERROR_INSUFFICIENT_BUFFER) {
      throw std::runtime_error("GetTokenInformation() failed: " + std::to_string(dwResult));
    }
  }

  TokenUserPtr user((PTOKEN_USER)LocalAlloc(GPTR, dw_size));
  if (user.get() == NULL) {
    throw std::runtime_error("LocalAlloc() failed: " + std::to_string(GetLastError()));
  }

  // Gets the user token from the security token (this one retrieves the actual user token)
  if (!GetTokenInformation(h_token, token_class, user.get(), dw_size, &dw_size)) {
    throw std::runtime_error("GetTokenInformation() failed: " + std::to_string(GetLastError()));
  }
  // Copies from the user token the SID
  DWORD dw_sid_len = GetLengthSid(user->User.Sid);
  pSID.reset((SID*)LocalAlloc(GPTR, dw_sid_len));
  CopySid(dw_sid_len, pSID.get(), user->User.Sid);
}

/**
 * Creates a file and makes it fully accessible by the current process user and
 * read only for LocalService account (which is the account under which the MySQL 
 * router runs as service). And not accessible for everyone else.
 */
static void make_file_private_win32(SECURITY_ATTRIBUTES& sa) {
  typedef std::unique_ptr<SECURITY_DESCRIPTOR, LocalFreeDeleter> SecurityDescriptorPtr;
  PACL new_dacl = NULL;
  SidPtr current_user((SID*)NULL);
  DWORD sid_size = SECURITY_MAX_SID_SIZE;
  SID local_service_sid;
  DWORD dw_res;
  // Obtains the SID of the LocalService account (the account under which runs 
  // the Router as a service in Windows)
  if (CreateWellKnownSid(WinLocalServiceSid, NULL, &local_service_sid, &sid_size) == FALSE) {
    throw std::runtime_error("CreateWellKnownSid() failed: " + std::to_string(GetLastError()));
  }
  // Retrieves the current user process SID.
  GetCurrentUserSid(current_user);

  // Sets the actual permissions: two ACEs (access control entries) (one for 
  // current user, one for LocalService) are configured and attached to a 
  // Security Descriptor's DACL (Discretionary Access Control List), then
  // the Security Descriptors is added to a Security Attributes struct, this 
  // last one is used in the API for file creation.
  EXPLICIT_ACCESSA ea[2];
  ZeroMemory(&ea, 2 * sizeof(EXPLICIT_ACCESSA));
  // Full acceess for current user
  ea[0].grfAccessPermissions = ACCESS_SYSTEM_SECURITY | READ_CONTROL | WRITE_DAC | GENERIC_ALL;
  ea[0].grfAccessMode = GRANT_ACCESS;
  ea[0].grfInheritance = NO_INHERITANCE;
  ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea[0].Trustee.ptstrName = reinterpret_cast<char*>(current_user.get());
  // Read only access for LocalService account
  ea[1].grfAccessPermissions = GENERIC_READ;
  ea[1].grfAccessMode = GRANT_ACCESS;
  ea[1].grfInheritance = NO_INHERITANCE;
  ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea[1].Trustee.ptstrName = reinterpret_cast<char*>(&local_service_sid);
  // Make a new DACL with the two ACEs
  dw_res = SetEntriesInAclA(2, ea, NULL, &new_dacl);

  try {
    if (ERROR_SUCCESS != dw_res) {
      throw std::runtime_error("SetEntriesInAcl() failed: " + std::to_string(dw_res));
    }

    // create and initialize a security descriptor.  
    SecurityDescriptorPtr psd((SECURITY_DESCRIPTOR*)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH));
    if (psd.get() == NULL) {
      throw std::runtime_error("LocalAlloc() failed: " + std::to_string(GetLastError()));
    }

    if (!InitializeSecurityDescriptor(psd.get(), SECURITY_DESCRIPTOR_REVISION)) {
      throw std::runtime_error("InitializeSecurityDescriptor failed: " + std::to_string(GetLastError()));
    }
    // attach the DACL to the security descriptor
    if (!SetSecurityDescriptorDacl(psd.get(), TRUE, new_dacl, FALSE)) {
      throw std::runtime_error("SetSecurityDescriptorDacl failed: " + std::to_string(GetLastError()));
    }
    
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = psd.get();
    sa.bInheritHandle = FALSE;
  } catch (...) {
    if (new_dacl != NULL)
      LocalFree((HLOCAL)new_dacl);
    throw;
  }
}


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
  SECURITY_ATTRIBUTES sa;
  try {
    make_file_private_win32(sa);
  } catch (std::runtime_error &e) {
    throw std::runtime_error("Could not set permissions for file '" + file_name + "': " + e.what());
  }
  // create the file with the desired security descriptor
  HANDLE h_file = CreateFileA(file_name.c_str(), GENERIC_ALL, 0, &sa, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h_file == INVALID_HANDLE_VALUE)
    throw std::runtime_error("Could not create the file '" + file_name + "'" + std::to_string(GetLastError()));
  CloseHandle(h_file);
#else
  std::ofstream f;
  file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
  try {
    f.open(file_name, std::ios_base::binary | std::ios_base::trunc | std::ios_base::out);
  } catch (std::runtime_error& e) {
    throw std::runtime_error("Could not create file '" + file_name + "':" + e.what());
  }
  try {
    throwing_chmod(file_name, S_IRUSR | S_IWUSR);
  } catch (std::runtime_error& e) {
    throw std::runtime_error("Could not set permissions for file '" + file_name + "': " + e.what());
  }
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
