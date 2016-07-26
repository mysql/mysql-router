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

#include "filesystem.h"

#include <ostream>

using std::string;

namespace mysql_harness {

////////////////////////////////////////////////////////////////
// class Path members and free functions

Path::Path() : type_(FileType::EMPTY_PATH) {}

Path::Path(const string& path) : path_(path), type_(FileType::TYPE_UNKNOWN) {
#ifdef _WIN32
  // in Windows, we normalize directory separator from \ to /, to not
  // confuse the rest of the code, which assume \ to be an escape char
  std::string::size_type p = path_.find('\\');
  while (p != std::string::npos) {
    path_[p] = '/';
    p = path_.find('\\');
  }
#endif
  string::size_type pos = path_.find_last_not_of(directory_separator);
  if (pos != string::npos)
    path_.erase(pos + 1);
  else if (path_.size() > 0)
    path_.erase(1);
  else
    throw std::invalid_argument("Empty path");
}

Path::Path(const char* path) : Path(string(path)) {}

void Path::validate_non_empty_path() const {
  if (!is_set()) {
    throw std::invalid_argument("Empty path");
  }
}

bool Path::operator==(const Path& rhs) const {
  return real_path().str() == rhs.real_path().str();
}


bool Path::operator<(const Path& rhs) const {
  return path_ < rhs.path_;
}


Path Path::basename() const {
  validate_non_empty_path();
  string::size_type pos = path_.find_last_of(directory_separator);
  if (pos == string::npos)
    return *this;
  else if (pos > 1)
    return string(path_, pos + 1);
  else
    return Path(root_directory);
}


Path Path::dirname() const {
  validate_non_empty_path();
  string::size_type pos = path_.find_last_of(directory_separator);
  if (pos == string::npos)
    return Path(".");
  else if (pos > 1)
    return string(path_, 0, pos);
  else
    return Path(root_directory);
}

bool Path::is_directory() const {
  validate_non_empty_path();
  return type() == FileType::DIRECTORY_FILE;
}

bool Path::is_regular() const {
  validate_non_empty_path();
  return type() == FileType::REGULAR_FILE;
}

bool Path::exists() const {
  validate_non_empty_path();
  return type() != FileType::FILE_NOT_FOUND
    && type() != FileType::STATUS_ERROR;
}

void Path::append(const Path& other) {
  validate_non_empty_path();
  other.validate_non_empty_path();
  path_.append(directory_separator + other.path_);
  type_ = FileType::TYPE_UNKNOWN;
}


Path Path::join(const Path& other) const {
  validate_non_empty_path();
  other.validate_non_empty_path();
  Path result(*this);
  result.append(other);
  return result;
}

std::ostream& operator<<(std::ostream& out, Path::FileType type) {
  static const char* type_names[]{
    "ERROR",
    "not found",
    "regular",
    "directory",
    "symlink",
    "block device",
    "character device",
    "FIFO",
    "socket",
    "UNKNOWN",
  };
  out << type_names[static_cast<int>(type)];
  return out;
}


///////////////////////////////////////////////////////////
// Directory::Iterator members

Directory::DirectoryIterator Directory::begin() {
  return DirectoryIterator(*this);
}


Directory::DirectoryIterator Directory::glob(const string& pattern) {
  return DirectoryIterator(*this, pattern);
}


Directory::DirectoryIterator Directory::end() {
  return DirectoryIterator();
}

///////////////////////////////////////////////////////////
// Directory members

Directory::~Directory() = default;

Directory::Directory(const Path& path) : Path(path) {}

}
