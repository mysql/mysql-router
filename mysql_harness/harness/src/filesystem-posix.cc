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

Path::Path(const char* path)
  : Path(std::string(path))
{
}

Path::Path(const std::string& path)
  : m_path(path)
  , m_type(FileType::TYPE_UNKNOWN)
{
  std::string::size_type pos = m_path.find_last_not_of(dirsep);
  if (pos != std::string::npos)
    m_path.erase(pos + 1);
  else if (m_path.size() > 0)
    m_path.erase(1);
  else
    throw std::runtime_error("Empty path");
}


Path::FileType
Path::type(bool refresh) const
{
  if (m_type == FileType::TYPE_UNKNOWN || refresh)
  {
    struct stat stat_buf;
    if (stat(c_str(), &stat_buf) == -1)
    {
      if (errno == ENOENT || errno == ENOTDIR)
        m_type = FileType::FILE_NOT_FOUND;
      else
        m_type = FileType::STATUS_ERROR;
    }
    else
      switch (stat_buf.st_mode & S_IFMT)
      {
      case S_IFDIR:
        m_type = FileType::DIRECTORY_FILE;
        break;
      case S_IFBLK:
        m_type = FileType::BLOCK_FILE;
        break;
      case S_IFCHR:
        m_type = FileType::CHARACTER_FILE;
        break;
      case S_IFIFO:
        m_type = FileType::FIFO_FILE;
        break;
      case S_IFLNK:
        m_type = FileType::SYMLINK_FILE;
        break;
      case S_IFREG:
        m_type = FileType::REGULAR_FILE;
        break;
      case S_IFSOCK:
        m_type = FileType::SOCKET_FILE;
        break;
      default:
        m_type = FileType::TYPE_UNKNOWN;
        break;
      }
  }
  return m_type;
}

bool Path::is_directory() const
{
  return type() == FileType::DIRECTORY_FILE;
}

bool Path::is_regular() const
{
  return type() == FileType::REGULAR_FILE;
}

void Path::append(const Path& other)
{
  m_path.append(dirsep + other.m_path);
  m_type = FileType::TYPE_UNKNOWN;
}


Path Path::join(const Path& other) const
{
  Path result(*this);
  result.append(other);
  return result;
}


Path Path::basename() const
{
  std::string::size_type pos = m_path.find_last_of(dirsep);
  if (pos == std::string::npos)
    return *this;
  else if (pos > 1)
    return std::string(m_path, pos + 1);
  else
    return Path("/");
}

Path Path::dirname() const
{
  std::string::size_type pos = m_path.find_last_of(dirsep);
  if (pos == std::string::npos)
    return Path(".");
  else if (pos > 1)
    return std::string(m_path, 0, pos);
  else
    return Path("/");
}

Directory::DirectoryIterator::DirectoryIterator(const Path& path,
                                                const std::string& pattern,
                                                struct dirent *result)
  : m_root(path)
  , m_dirp(opendir(path.c_str()))
  , m_result(result)
  , m_pattern(pattern)
{
  if (m_dirp == nullptr)
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
  : DirectoryIterator(path, pattern, &m_entry)
{

}

void
Directory::DirectoryIterator::fill_result()
{
  // This is similar to scandir(2), but we do not use scandir(2) since
  // we want to be thread-safe.

  if (m_result == nullptr)
    return;

  while (true)
  {
    if (int error = readdir_r(m_dirp, &m_entry, &m_result))
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
    if (m_result == nullptr)
      break;

    // Skip current directory and parent directory.
    if (strcmp(m_result->d_name, ".") == 0 ||
        strcmp(m_result->d_name, "..") == 0)
      continue;

    // If no pattern is given, we're done.
    if (m_pattern.size() == 0)
      break;

    // Skip any entries that do not match the pattern
    int error = fnmatch(m_pattern.c_str(), m_result->d_name, FNM_PATHNAME);
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
  assert(m_result);
  return m_root.join(m_result->d_name);
}

bool
Directory::DirectoryIterator::operator!=(const DirectoryIterator& rhs)
{
  return m_result != rhs.m_result;
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
