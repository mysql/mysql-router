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
#include <sstream>
#include <stdexcept>

#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200112L
# error "This file expects POSIX.1-2001 or later"
#endif

using std::ostringstream;

namespace {
  const std::string dirsep("/");
  const std::string extsep(".");
}

void Path::validate_non_empty_path() const {
  if (type_ == FileType::EMPTY_PATH)
    throw std::invalid_argument("Empty path");
}

Path::Path()
    : type_(FileType::EMPTY_PATH)
{
}

Path::Path(const char* path)
    : Path(std::string(path))
{
}

Path::Path(const std::string& path)
    : path_(path)
    , type_(FileType::TYPE_UNKNOWN)
{
  std::string::size_type pos = path_.find_last_not_of(dirsep);
  if (pos != std::string::npos)
    path_.erase(pos + 1);
  else if (path_.size() > 0)
    path_.erase(1);
  else
    throw std::invalid_argument("Empty path");
}


Path::FileType
Path::type(bool refresh) const
{
  validate_non_empty_path();
  if (type_ == FileType::TYPE_UNKNOWN || refresh)
  {
    struct stat stat_buf;
    if (stat(c_str(), &stat_buf) == -1)
    {
      if (errno == ENOENT || errno == ENOTDIR)
        type_ = FileType::FILE_NOT_FOUND;
      else
        type_ = FileType::STATUS_ERROR;
    }
    else
      switch (stat_buf.st_mode & S_IFMT)
      {
      case S_IFDIR:
        type_ = FileType::DIRECTORY_FILE;
        break;
      case S_IFBLK:
        type_ = FileType::BLOCK_FILE;
        break;
      case S_IFCHR:
        type_ = FileType::CHARACTER_FILE;
        break;
      case S_IFIFO:
        type_ = FileType::FIFO_FILE;
        break;
      case S_IFLNK:
        type_ = FileType::SYMLINK_FILE;
        break;
      case S_IFREG:
        type_ = FileType::REGULAR_FILE;
        break;
      case S_IFSOCK:
        type_ = FileType::SOCKET_FILE;
        break;
      default:
        type_ = FileType::TYPE_UNKNOWN;
        break;
      }
  }
  return type_;
}

bool Path::is_directory() const
{
  validate_non_empty_path();
  return type() == FileType::DIRECTORY_FILE;
}

bool Path::is_regular() const
{
  validate_non_empty_path();
  return type() == FileType::REGULAR_FILE;
}

void Path::append(const Path& other)
{
  validate_non_empty_path();
  other.validate_non_empty_path();
  path_.append(dirsep + other.path_);
  type_ = FileType::TYPE_UNKNOWN;
}


Path Path::join(const Path& other) const
{
  validate_non_empty_path();
  other.validate_non_empty_path();
  Path result(*this);
  result.append(other);
  return result;
}


Path Path::basename() const
{
  validate_non_empty_path();
  std::string::size_type pos = path_.find_last_of(dirsep);
  if (pos == std::string::npos)
    return *this;
  else if (pos > 1)
    return std::string(path_, pos + 1);
  else
    return Path("/");
}

Path Path::dirname() const
{
  validate_non_empty_path();
  std::string::size_type pos = path_.find_last_of(dirsep);
  if (pos == std::string::npos)
    return Path(".");
  else if (pos > 1)
    return std::string(path_, 0, pos);
  else
    return Path("/");
}

Directory::DirectoryIterator::DirectoryIterator(const Path& path,
                                                const std::string& pattern,
                                                struct dirent *result)
  : root_(path)
  , dirp_(opendir(path.c_str()))
  , result_(result)
  , pattern_(pattern)
{
  if (dirp_ == nullptr)
  {
    ostringstream  buffer;
    char msg[256];
    if (strerror_r(errno, msg, sizeof(msg)))
      buffer << "strerror_r failed: " << errno;
    else
      buffer << "Failed to open path " << path << " - " << msg;
    throw std::runtime_error(buffer.str());
  }

  fill_result();
}

Directory::DirectoryIterator::DirectoryIterator(const Path& path,
                                                const std::string& pattern)
  : DirectoryIterator(path, pattern, &entry_)
{

}

void
Directory::DirectoryIterator::fill_result()
{
  // This is similar to scandir(2), but we do not use scandir(2) since
  // we want to be thread-safe.

  if (result_ == nullptr)
    return;

  while (true)
  {
    if (int error = readdir_r(dirp_, &entry_, &result_))
    {
      ostringstream buffer;
      char msg[256];
      if (strerror_r(error, msg, sizeof(msg)))
        buffer << "strerror_r failed: " << errno;
      else
        buffer << "Failed to read directory entry - " << msg;
      throw std::runtime_error(buffer.str());
    }

    // If there are no more entries, we're done.
    if (result_ == nullptr)
      break;

    // Skip current directory and parent directory.
    if (strcmp(result_->d_name, ".") == 0 ||
        strcmp(result_->d_name, "..") == 0)
      continue;

    // If no pattern is given, we're done.
    if (pattern_.size() == 0)
      break;

    // Skip any entries that do not match the pattern
    int error = fnmatch(pattern_.c_str(), result_->d_name, FNM_PATHNAME);
    if (error == FNM_NOMATCH)
      continue;
    else if (error == 0)
      break;
    else
    {
      ostringstream buffer;
      char msg[256];
      if (strerror_r(error, msg, sizeof(msg)))
        buffer << "strerror_r failed: " << errno;
      else
        buffer << "Match failed - " << msg;
      throw std::runtime_error(buffer.str());
    }
  }
}

Directory::DirectoryIterator&
Directory::DirectoryIterator::operator++()
{
  fill_result();
  return *this;
}

Path
Directory::DirectoryIterator::operator*() const
{
  assert(result_);
  return root_.join(result_->d_name);
}

bool
Directory::DirectoryIterator::operator!=(const DirectoryIterator& rhs)
{
  return result_ != rhs.result_;
}

Directory::DirectoryIterator
Directory::begin()
{
  return DirectoryIterator(*this);
}

Directory::DirectoryIterator
Directory::glob(const std::string& pattern)
{
  return DirectoryIterator(*this, pattern);
}

Directory::DirectoryIterator
Directory::end()
{
  return DirectoryIterator(*this, "", nullptr);
}


std::ostream&
operator<<(std::ostream& out, Path::FileType type)
{
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


Path
Path::make_path(const Path& dir,
                const std::string& base,
                const std::string& ext)
{
  return dir.join(base + extsep + ext);
}
