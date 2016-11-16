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

#include "utils.h"
#include "filesystem.h"
#include "common.h"
#include "mysqlrouter/utils.h"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <cctype>
#include <climits>
#include <stdexcept>
#include <functional>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <termios.h>
#include <unistd.h>
#else
#include <windows.h>
#include <direct.h>
#include <io.h>
#endif

using std::string;

const string kValidIPv6Chars = "abcdefgABCDEFG0123456789:";
const string kValidPortChars = "0123456789";

namespace mysqlrouter {

std::vector<string> wrap_string(const string &to_wrap, size_t width, size_t indent_size) {
  size_t curr_pos = 0;
  size_t wrap_pos = 0;
  size_t prev_pos = 0;
  string work{to_wrap};
  std::vector<string> res{};
  auto indent = string(indent_size, ' ');
  auto real_width = width - indent_size;

  size_t str_size = work.size();
  if (str_size < real_width) {
    res.push_back(indent + work);
  } else {
    work.erase(std::remove(work.begin(), work.end(), '\r'), work.end());
    std::replace(work.begin(), work.end(), '\t', ' '), work.end();
    str_size = work.size();

    do {
      curr_pos = prev_pos + real_width;

      // respect forcing newline
      wrap_pos = work.find("\n", prev_pos);
      if (wrap_pos == string::npos || wrap_pos > curr_pos) {
        // No new line found till real_width
        wrap_pos = work.find_last_of(" ", curr_pos);
      }
      if (wrap_pos != string::npos) {
        res.push_back(indent + work.substr(prev_pos, wrap_pos - prev_pos));
        prev_pos = wrap_pos + 1;  // + 1 to skip space
      } else {
        break;
      }
    } while (str_size - prev_pos > real_width || work.find("\n", prev_pos) != string::npos);
    res.push_back(indent + work.substr(prev_pos));
  }

  return res;
}

bool my_check_access(const std::string& path) {
#ifndef _WIN32
  return (access(path.c_str(), R_OK | X_OK) == 0);
#else
  return (_access(path.c_str(), 0x04) == 0);
#endif
}

void copy_file(const std::string &from, const std::string &to) {
  std::ofstream ofile;
  std::ifstream ifile;

  ofile.open(to, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
  if (ofile.fail()) {
    throw std::runtime_error("Could not create file '" + to + "': " +
                             mysql_harness::get_strerror(errno));
  }
  ifile.open(from, std::ofstream::in | std::ofstream::binary);
  if (ifile.fail()) {
    throw std::runtime_error("Could not open file '" + from + "': " +
                             mysql_harness::get_strerror(errno));
  }

  ofile << ifile.rdbuf();

  ofile.close();
  ifile.close();
}

int mkdir(const std::string& dir, int mode) {
#ifndef _WIN32
  return ::mkdir(dir.c_str(), static_cast<mode_t>(mode));
#else
  return _mkdir(dir.c_str());
#endif
}

int rmdir(const std::string& dir) {
#ifndef _WIN32
  return ::rmdir(dir.c_str());
#else
  return _rmdir(dir.c_str());
#endif
}

int delete_file(const std::string& path) {
#ifndef _WIN32
  return ::unlink(path.c_str());
#else
  return DeleteFile(path.c_str()) ? 0 : -1;
#endif
}

int delete_recursive(const std::string& dir) {
  mysql_harness::Directory d(dir);
  try {
    for (auto const &f : d) {
      if (f.is_directory()) {
        if (delete_recursive(f.str()) < 0)
          return -1;
      } else {
        if (delete_file(f.str()) < 0)
          return -1;
      }
    }
  } catch (...) {
    return -1;
  }
  return rmdir(dir);
}

bool substitute_envvar(std::string &line) noexcept {
  size_t pos_start;
  size_t pos_end;

  pos_start = line.find("ENV{");
  if (pos_start == string::npos) {
    return true;  // no environment variable placeholder found -> this is not an error, just a no-op
  }

  pos_end = line.find("}", pos_start + 4);
  if (pos_end == string::npos) {
    return false; // environment placeholder not closed (missing '}')
  }

  string env_var = line.substr(pos_start + 4, pos_end - pos_start - 4);
  if (env_var.empty()) {
    return false; // no environment variable name found in placeholder
  }

  const char* env_var_value = std::getenv(env_var.c_str());
  if (env_var_value == nullptr) {
    return false; // unknown environment variable
  }

  // substitute the variable and return success
  line.replace(pos_start, env_var.size() + 5, env_var_value);
  return true;
}

string string_format(const char *format, ...) {

  va_list args;
  va_start(args, format);
  va_list args_next;
  va_copy(args_next, args);

  int size = std::vsnprintf(nullptr, 0, format, args);
  std::vector<char> buf(static_cast<size_t>(size) + 1U);
  va_end(args);

  std::vsnprintf(buf.data(), buf.size(), format, args_next);
  va_end(args_next);

  return string(buf.begin(), buf.end() - 1);
}

std::pair<string, uint16_t> split_addr_port(string data) {
  size_t pos;
  string addr;
  uint16_t port = 0;
  trim(data);

  if (data.at(0) == '[') {
    // IPv6 with port
    pos = data.find(']');
    if (pos == string::npos) {
      throw std::runtime_error("invalid IPv6 address: missing closing square bracket");
    }
    addr.assign(data, 1, pos - 1);
    if (addr.find_first_not_of(kValidIPv6Chars) != string::npos) {
      throw std::runtime_error("invalid IPv6 address: illegal character(s)");
    }
    pos = data.find(":", pos);
    if (pos != string::npos) {
      try {
        port = get_tcp_port(data.substr(pos + 1));
      } catch (const std::runtime_error &exc) {
        throw std::runtime_error("invalid TCP port: " + string(exc.what()));
      }
    }
  } else if (std::count(data.begin(), data.end(), ':') > 1) {
    // IPv6 without port
    pos = data.find(']');
    if (pos != string::npos) {
      throw std::runtime_error("invalid IPv6 address: missing opening square bracket");
    }
    if (data.find_first_not_of(kValidIPv6Chars) != string::npos) {
      throw std::runtime_error("invalid IPv6 address: illegal character(s)");
    }
    addr.assign(data);
  } else {
    // IPv4 or address
    pos = data.find(":");
    addr = data.substr(0, pos);
    if (pos != string::npos) {
      try {
        port = get_tcp_port(data.substr(pos + 1));
      } catch (const std::runtime_error &exc) {
        throw std::runtime_error("invalid TCP port: " + string(exc.what()));
      }
    }
  }

  return std::make_pair(addr, port);
}

uint16_t get_tcp_port(const string &data) {
  int port;

  // We refuse data which is bigger than 5 characters
  if (data.find_first_not_of(kValidPortChars) != string::npos || data.size() > 5) {
    throw std::runtime_error("invalid characters or too long");
  }

  try {
    port = data.empty() ? 0 : static_cast<int>(std::strtol(data.c_str(), nullptr, 10));
  } catch (const std::invalid_argument &exc) {
    throw std::runtime_error("convertion to integer failed");
  } catch (const std::out_of_range &exc) {
    throw std::runtime_error("impossible port number (out-of-range)");
  }

  if (port > UINT16_MAX) {
    throw std::runtime_error("impossible port number");
  }
  return static_cast<uint16_t>(port);
}

std::vector<string> split_string(const string& data, const char delimiter, bool allow_empty) {
  std::stringstream ss(data);
  std::string token;
  std::vector<string> result;

  if (data.empty()) {
    return {};
  }

  while (std::getline(ss, token, delimiter)) {
    if (token.empty() && !allow_empty) {
      // Skip empty
      continue;
    }
    result.push_back(token);
  }

  // When last character is delimiter, it denotes an empty token
  if (allow_empty && data.back() == delimiter) {
    result.push_back("");
  }

  return result;
}

void left_trim(string& str) {
  str.erase(str.begin(), std::find_if_not(str.begin(), str.end(), ::isspace));
}

void right_trim(string& str) {
  str.erase(std::find_if_not(str.rbegin(), str.rend(), ::isspace).base(), str.end());
}

void trim(string& str) {
  left_trim(str);
  right_trim(str);
}

string hexdump(const unsigned char *buffer, size_t count, long start, bool literals) {
  std::ostringstream os;

  using std::setfill;
  using std::setw;
  using std::hex;

  int w = 16;
  buffer += start;
  size_t n = 0;
  for (const unsigned char *ptr = buffer; n < count; ++n, ++ptr ) {
    if (literals && ((*ptr >= 0x41 && *ptr <= 0x5a) || (*ptr >= 61 && *ptr <= 0x7a))) {
      os << setfill(' ') << setw(2) << *ptr;
    } else {
      os << setfill('0') << setw(2) << hex << static_cast<int>(*ptr);
    }
    if (w == 1) {
      os << std::endl;
      w = 16;
    } else {
      os << " ";
      --w;
    }
  }
  // Make sure there is always a new line on the last line
  if (w < 16) {
    os << std::endl;
  }
  return os.str();
}

/*
* Returns the last system specific error description (using GetLastError in Windows or errno in Unix/OSX).
*/
std::string get_last_error(int myerrnum)
{
#ifdef WIN32
  DWORD dwCode = myerrnum ? myerrnum : GetLastError();
  LPTSTR lpMsgBuf;

  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    dwCode,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR)&lpMsgBuf,
    0, NULL);
  std::string msgerr = "SystemError: ";
  msgerr += lpMsgBuf;
  msgerr += "with error code %d.";
  std::string result = string_format(msgerr.c_str(), dwCode);
  LocalFree(lpMsgBuf);
  return result;
#else
  char sys_err[64];
  int errnum = myerrnum ? myerrnum : errno;

  sys_err[0] = 0; // init, in case strerror_r() fails

  // we do this #ifdef dance because on unix systems strerror_r() will generate
  // a warning if we don't collect the result (warn_unused_result attribute)
#if ((defined _POSIX_C_SOURCE && (_POSIX_C_SOURCE >= 200112L)) ||    \
       (defined _XOPEN_SOURCE && (_XOPEN_SOURCE >= 600)))      &&    \
      ! defined _GNU_SOURCE
  int r = strerror_r(errno, sys_err, sizeof(sys_err));
  (void)r;  // silence unused variable;
#elif defined _GNU_SOURCE
  const char *r = strerror_r(errno, sys_err, sizeof(sys_err));
  (void)r;  // silence unused variable;
#else
  strerror_r(errno, sys_err, sizeof(sys_err));
#endif

  std::string s = sys_err;
  s += "with errno %d.";
  std::string result = string_format(s.c_str(), errnum);
  return result;
#endif
  }

#ifndef _WIN32
static string default_prompt_password(const string &prompt) {
  struct termios console;
  tcgetattr(STDIN_FILENO, &console);

  std::cout << prompt << ": ";

  // prevent showing input
  console.c_lflag &= ~(uint)ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &console);

  string result;
  std::getline(std::cin, result);

  // reset
  console.c_lflag |= ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &console);

  std::cout << std::endl;
  return result;
}
#else
static string default_prompt_password(const string &prompt) {

  std::cout << prompt << ": ";

  // prevent showing input
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode;
  GetConsoleMode(hStdin, &mode);
  mode &= ~ENABLE_ECHO_INPUT;
  SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));

  string result;
  std::getline(std::cin, result);

  // reset
  SetConsoleMode(hStdin, mode);

  std::cout << std::endl;
  return result;
}
#endif

static std::function<string (const string&)> g_prompt_password = default_prompt_password;

void set_prompt_password(const std::function<std::string (const std::string &)> &f) {
  g_prompt_password = f;
}

string prompt_password(const std::string &prompt) {
  return g_prompt_password(prompt);
}


#ifdef _WIN32
bool is_running_as_service() {
  bool result = false;

  HWINSTA h = GetProcessWindowStation();
  if (h != NULL) {
    USEROBJECTFLAGS uof = { 0 };
    if (GetUserObjectInformation(h, UOI_FLAGS, &uof, sizeof(USEROBJECTFLAGS), NULL)
      && !(uof.dwFlags & WSF_VISIBLE)) {
      result = true;
    }
  }
  return result;
}

void write_windows_event_log(const std::string& msg) {
  static const std::string event_source_name = "MySQL Router";
  HANDLE event_src = NULL;
  LPCSTR strings[2] = { NULL, NULL };
  event_src = RegisterEventSourceA(NULL, event_source_name.c_str());
  if (event_src) {
    strings[0] = event_source_name.c_str();
    strings[1] = msg.c_str();
    ReportEventA(event_src, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 2, 0, strings, NULL);
    DeregisterEventSource(event_src);
  } else {
    throw std::runtime_error("Cannot create event log source, error: " + std::to_string(GetLastError()));
  }
}
#endif

bool is_valid_socket_name(const std::string &socket, std::string &err_msg) {
  bool result = true;

#ifndef _WIN32
  result = socket.size() <= (sizeof(sockaddr_un().sun_path)-1);
  err_msg =  "Socket file path can be at most "
           + to_string(sizeof(sockaddr_un().sun_path)-1)
           + " characters (was " + to_string(socket.size()) + ")";
#endif

  return result;
}

int strtoi_checked(const char* value, const int default_value) {
  if (value == nullptr)
    return default_value;

  char* tmp {nullptr};
  // NOTE: we need to play with errno here as it is not enough to check
  // for LONG_MIN, LONG_MAX as these are still valid values and ERANGE
  // can be the result of some previous operation
  auto old_errno = errno;
  errno = 0;

  auto result = std::strtol(value, &tmp, 10);

  // if our operation did not set the errno let's be kind enough
  // to restore it's old value
  auto our_errno = errno;
  if (errno == 0) {
    errno = old_errno;
  }

  // check if the conversion was valid
  if (value == tmp || *tmp != '\0' || our_errno == ERANGE) {
    return default_value;
  }

  // check if the value fits int
  if (result < static_cast<long>(INT_MIN) || result > static_cast<long>(INT_MAX)) {
    return default_value;
  }

  return static_cast<int>(result);
}


} // namespace mysqlrouter
