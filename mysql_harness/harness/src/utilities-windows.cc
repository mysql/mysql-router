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

#include "utilities.h"

#include <Windows.h>
#include <Winsock2.h>
#include <Shlwapi.h>

#include <string>

namespace mysql_harness {

namespace utility {

bool matches_glob(const std::string& word, const std::string& pattern) {
  return PathMatchSpec(word.c_str(), pattern.c_str());
}

void sleep_seconds(unsigned int seconds) {
  Sleep(1000 * seconds);
}

std::string get_message_error(int errcode) {
  if (errcode == SOCKET_ERROR || errcode == 0) {
    errcode = WSAGetLastError();
  }

  LPTSTR lpMsgBuf;

  if (0 != FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER |
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      errcode,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR)&lpMsgBuf,
      0, NULL)) {
    std::string msgerr = "SystemError: ";
    msgerr += lpMsgBuf;
    LocalFree(lpMsgBuf);
    return msgerr;
  } else {
    return "SystemError: " + std::to_string(errcode);
  }
}

}  // namespace utility

} // namespace mysql_harness
