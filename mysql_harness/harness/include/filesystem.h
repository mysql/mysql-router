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

#ifndef FILESYSTEM_INCLUDED
#define FILESYSTEM_INCLUDED

#include <string>

#include <dirent.h>

/**
 * @defgroup Filesystem Platform-independent file system operations
 *
 * Abstractions for file system operations.
 */

/**
 * Class representing a path in a file system.
 *
 * Paths are used to access files in the file system and can be either
 * relative or absolute. Absolute paths have a slash (`/`) first in
 * the path, otherwise, the path is relative.
 */
class Path {
  friend std::ostream& operator<<(std::ostream& out, const Path& path) {
    out << path.path_;
    return out;
  }

public:
  /**
   * Enum used to identify file types.
   */

  enum class FileType {
    /** An error occured when trying to get file type, but it is *not*
     * that the file was not found. */
    STATUS_ERROR,

    /** Empty path was given */
    EMPTY_PATH,

    /** The file was not found. */
    FILE_NOT_FOUND,

    /** The file is a regular file. */
    REGULAR_FILE,

    /** The file is a directory. */
    DIRECTORY_FILE,

    /** The file is a symbolic link. */
    SYMLINK_FILE,

    /** The file is a block device */
    BLOCK_FILE,

    /** The file is a character device */
    CHARACTER_FILE,

    /** The file is a FIFO */
    FIFO_FILE,

    /** The file is a UNIX socket */
    SOCKET_FILE,

    /** The type of the file is unknown, either because it was not
     * fetched yet or because stat(2) reported something else than the
     * above. */
    TYPE_UNKNOWN,
  };

  friend std::ostream& operator<<(std::ostream& out, FileType type);

  /**
   * Construct a path
   *
   * @param path Non-empty string denoting the path.
   */
  Path();

  /** @overload */
  Path(const std::string& path);

  /** @overload */
  Path(const char* path);

  /**
   * Create a path from directory, basename, and extension.
   */
  static Path make_path(const Path& directory,
                        const std::string& basename,
                        const std::string& extension);

  bool operator==(const Path& rhs) const {
    return path_ == rhs.path_;
  }

  /**
   * Path ordering operator.
   *
   * This is mainly used for ordered containers. The paths are ordered
   * lexicographically.
   */
  bool operator<(const Path& rhs) const {
    return path_ < rhs.path_;
  }

  /**
   * Get the file type.
   *
   * The file type is normally cached so if the file type under a path
   * changes it is necessary to force a refresh.
   *
   * @param refresh Set to `true` if the file type should be
   * refreshed, default to `false`.
   *
   * @return The type of the file.
   */
  FileType type(bool refresh = false) const;

  /**
   * Check if the file is a directory.
   */
  bool is_directory() const;

  /**
   * Check if the file is a regular file.
   */
  bool is_regular() const;

  /**
   * Get the directory name of the path.
   *
   * This will strip the last component of a path, assuming that the
   * what remains is a directory name. If the path is a relative path
   * that do not contain any directory separators, a dot will be
   * returned (denoting the current directory).
   *
   * @note No checking of the components are done, this is just simple
   * path manipulation.
   *
   * @return A new path object representing the directory portion of
   * the path.
   */
  Path dirname() const;

  /**
   * Get the basename of the path.
   *
   * Return the basename of the path: the path without the directory
   * portion.
   *
   * @note No checking of the components are done, this is just simple
   * path manipulation.
   *
   * @return A new path object representing the basename of the path.
   * the path.
   */
  Path basename() const;

  /**
   * Append a path component to the current path.
   *
   * This function will append a path component to the path using the
   * apropriate directory separator.
   *
   * @param other Path component to append to the path.
   */
  void append(const Path& other);


  /**
   * Join two path components to form a new path.
   *
   * This function will join the two path components using a
   * directory separator.
   *
   * @note This will return a new `Path` object. If you want to modify
   * the existing path object, you should use `append` instead.
   *
   * @param other Path component to be appended to the path
   */
  Path join(const Path& other) const;

  /** @overload */
  Path join(const char* other) const { return join(Path(other)); }


  /**
   * Get a C-string representation to the path.
   *
   * @note This will return a pointer to the internal representation
   * of the path and hence will become a dangling pointer when the
   * `Path` object is destroyed.
   *
   * @return Pointer to a null-terminated C-string.
   */
  const char *c_str() const {
    return path_.c_str();
  }

  /**
   * Get a string representation of the path.
   *
   * @return Instance of std::string containing the path.
   */
  const std::string& str() const noexcept {
    return path_;
  }

private:
  void validate_non_empty_path() const;

  std::string path_;
  mutable FileType type_;
};

/**
 * Class representing a directory in a file system.
 *
 * In addition to being a refinement of `Path`, it also have functions
 * that make it act like a container of paths and support iterating
 * over the entries in a directory.
 *
 * An example of how it could be used is:
 * @code
 * for (auto&& entry: Directory(path))
 *   std::cout << entry << std::endl;
 * @endcode
 */
class Directory
  : public Path
{
public:
  /**
   * Directory iterator for iterating over directory entries.
   *
   * A directory iterator is an input iterator.
   */
  class DirectoryIterator
    : public std::iterator<std::input_iterator_tag, Path>
  {
    friend class Directory;

    DirectoryIterator(const Path& path,
                      const std::string& pattern = std::string());
    DirectoryIterator(const Path& path, const std::string& glob,
                      struct dirent *result);

  public:
    /** Standard iterator operators */
    /** @{ */
    Path operator*() const;
    DirectoryIterator& operator++();
    Path operator->() { return this->operator*(); }
    bool operator!=(const DirectoryIterator& other);
    /** @} */

  private:
    void fill_result();

    const Path root_;

    // This is POSIX-specific, it need to be factored out into an
    // implementation class that can vary between platforms.
    DIR *dirp_;
    struct dirent entry_;
    struct dirent *result_;
    std::string pattern_;
  };

  /**
   * Construct a directory instance.
   * 
   * Construct a directory instance in different ways depending on the version of the constructor used.
   */
  Directory(const std::string& path)
    : Path(path)
  {
  }

  /** @overload */
  Directory(const Path& path)
    : Path(path)
  {
  }

  /**
   * Iterator to first entry.
   *
   * @return Returns an iterator pointing to the first entry.
   */
  DirectoryIterator begin();

  /**
   * Iterator past-the-end of entries.
   *
   * @return Returns an iterator pointing *past-the-end* of the entries.
   */
  DirectoryIterator end();

  /**
   * Iterate over entries matching a glob.
   */
  DirectoryIterator glob(const std::string& glob);
};

#endif /* FILESYSTEM_INCLUDED */
