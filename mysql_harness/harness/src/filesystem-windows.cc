/*
Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "filesystem.h"

#include <cassert>
#include <cerrno>
#include <sstream>
#include <stdexcept>
#include <string>

#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>
#include <shlwapi.h>

using std::string;
using std::ostringstream;
using std::runtime_error;

namespace {
  std::string get_last_error() {
    char message[512];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM
      | FORMAT_MESSAGE_IGNORE_INSERTS
      | FORMAT_MESSAGE_ALLOCATE_BUFFER,
      nullptr, GetLastError(),
      LANG_NEUTRAL, message, sizeof(message),
      nullptr);
    return std::string(message);
  }
  const std::string dirsep("/");
  const std::string extsep(".");
}

namespace mysql_harness {

////////////////////////////////////////////////////////////////
// class Path members and free functions

// We normalize the Path class to use / internally, to avoid problems
// with code that assume \ to be an escape character
const char * const Path::directory_separator = "/";
const char * const Path::root_directory = "/";

Path::FileType Path::type(bool refresh) const {
  validate_non_empty_path();
  if (type_ == FileType::TYPE_UNKNOWN || refresh) {
    struct _stat stat_buf;
    if (_stat(c_str(), &stat_buf) == -1) {
      if (errno == ENOENT)
        type_ = FileType::FILE_NOT_FOUND;
      else if (errno == EINVAL)
        type_ = FileType::STATUS_ERROR;
    } else {
      switch (stat_buf.st_mode & S_IFMT) {
      case S_IFDIR:
        type_ = FileType::DIRECTORY_FILE;
        break;
      case S_IFCHR:
        type_ = FileType::CHARACTER_FILE;
        break;
      case S_IFREG:
        type_ = FileType::REGULAR_FILE;
        break;
      default:
        type_ = FileType::TYPE_UNKNOWN;
        break;
      }
    }
  }
  return type_;
}

////////////////////////////////////////////////////////////////
// Directory::Iterator::State

class Directory::DirectoryIterator::State {
 public:
  State();
  State(const Path& path, const string& pattern);
  ~State();

  void fill_result();

  template <typename IteratorType>
  static bool equal(const IteratorType& lhs, const IteratorType& rhs) {
    assert(lhs != nullptr && rhs != nullptr);

    // If either interator is an end iterator, they are equal if both
    // are end iterators.
    if (!lhs->more_ || !rhs->more_)
      return lhs->more_ == rhs->more_;

    // Otherwise, they are not equal (since we are using input
    // iterators, they do not compare equal in any other cases).
    return false;
  }

  WIN32_FIND_DATA data_;
  HANDLE handle_;
  bool more_;
  const string pattern_;

 private:
  static const char* dot;
  static const char* dotdot;
};

const char* Directory::DirectoryIterator::State::dot = ".";
const char* Directory::DirectoryIterator::State::dotdot = "..";

Directory::DirectoryIterator::State::State()
  : handle_(INVALID_HANDLE_VALUE), more_(false), pattern_("") {}

Directory::DirectoryIterator::State::State(const Path& path,
                                  const string& pattern)
  : handle_(INVALID_HANDLE_VALUE), more_(true), pattern_(pattern) {
  const Path r_path = path.real_path();
  const string pat = r_path.join(pattern.size() > 0 ? pattern : "*").str();

  if (pat.size() > MAX_PATH) {
    ostringstream  buffer;
    buffer << "Failed to open path " << path << " - " << "path too long";
    throw runtime_error(buffer.str());
  }

  handle_ = FindFirstFile(pat.c_str(), &data_);
  bool first = true;
  if (handle_ != INVALID_HANDLE_VALUE) {
    more_ = true;

    while (more_) {
      if (first)
        first = false;
      else
        more_ = (FindNextFile(handle_, &data_) != 0);
      if (!more_) {
        int error = GetLastError();
        if (error != ERROR_NO_MORE_FILES) {
          ostringstream buffer;
          buffer << "Failed to read directory entry - "
            << get_last_error();
          throw runtime_error(buffer.str());
        }
      } else {
        // Skip current directory and parent directory.
        if (!strcmp(data_.cFileName, dot) || !strcmp(data_.cFileName, dotdot))
          continue;

        // If no pattern is given, we're done.
        if (pattern_.size() == 0)
          break;

        BOOL result = PathMatchSpecA(data_.cFileName, pattern_.c_str());
        if (!result)
          continue;
        else
          break;
      }
    }
  } else {
    throw runtime_error("FindFirstFile - " + get_last_error());
  }
}

Directory::DirectoryIterator::State::~State() {
  if (handle_ != INVALID_HANDLE_VALUE)
    FindClose(handle_);
}


void Directory::DirectoryIterator::State::fill_result() {
  assert(handle_ != INVALID_HANDLE_VALUE);
  while (true) {
    more_ = (FindNextFile(handle_, &data_) != 0);
    if (!more_) {
      int error = GetLastError();
      if (error != ERROR_NO_MORE_FILES) {
        ostringstream buffer;
        buffer << "Failed to read directory entry - "
               << get_last_error();
        throw runtime_error(buffer.str());
      } else {
        break;
      }
    } else {
      // Skip current directory and parent directory.
      if (!strcmp(data_.cFileName, dot) || !strcmp(data_.cFileName, dotdot))
        continue;

      // If no pattern is given, we're done.
      if (pattern_.size() == 0)
        break;

      // Skip any entries that do not match the pattern
      BOOL result = PathMatchSpecA(data_.cFileName, pattern_.c_str());
      if (!result)
        continue;
      else
        break;
    }
  }
}


////////////////////////////////////////////////////////////////
// Directory::Iterator

// These definition of the default constructor and destructor need to
// be here since the automatically generated default
// constructor/destructor uses the definition of the class 'State',
// which is not available when the header file is read.
#if !defined(_MSC_VER) || (_MSC_VER >= 1900)
Directory::DirectoryIterator::~DirectoryIterator() = default;
Directory::DirectoryIterator::DirectoryIterator(
    DirectoryIterator&&) = default;
Directory::DirectoryIterator::DirectoryIterator(
    const DirectoryIterator&) = default;
#elif defined(_MSC_VER)
Directory::DirectoryIterator::~DirectoryIterator() {
  state_.reset();
}
#endif

Directory::DirectoryIterator::DirectoryIterator()
  : path_("*END*"), state_(std::make_shared<State>()) {}


Directory::DirectoryIterator::DirectoryIterator(const Path& path,
                                                const std::string& pattern)
  : path_(path.real_path()), state_(std::make_shared<State>(path, pattern)) {}


Directory::DirectoryIterator& Directory::DirectoryIterator::operator++() {
  assert(state_ != nullptr);
  state_->fill_result();
  return *this;
}

Path Directory::DirectoryIterator::operator*() const {
  assert(state_ != nullptr && state_->handle_ != INVALID_HANDLE_VALUE);
  return path_.join(state_->data_.cFileName);
}

bool
Directory::DirectoryIterator::operator!=(const DirectoryIterator& rhs) const {
  return !State::equal(state_, rhs.state_);
}

Path
Path::make_path(const Path& dir,
                const std::string& base,
                const std::string& ext) {
  return dir.join(base + extsep + ext);
}

Path Path::real_path() const {
  validate_non_empty_path();

  // store a copy of str() in native_path
  assert(0 < str().size() && str().size() < MAX_PATH);
  char native_path[MAX_PATH];
  std::memcpy(native_path, c_str(), str().size() + 1);  // +1 for string terminator

  // replace all '/' with '\'
  char* p = native_path;
  while (*p) {
    if (*p == '/') {
      *p = '\\';
    }
    p++;
  }

  // resolve absolute path
  char path[MAX_PATH];
  if (GetFullPathNameA(native_path, sizeof(path), path, nullptr) == 0) {
    return Path();
  }

  // check if the path exists, to match posix behaviour
  WIN32_FIND_DATA find_data;
  HANDLE h = FindFirstFile(native_path, &find_data);
  if (h == INVALID_HANDLE_VALUE) {
    return Path();
  }
  FindClose(h);

  return Path(path);
}

}
