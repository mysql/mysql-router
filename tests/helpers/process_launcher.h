/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef _PROCESS_LAUNCHER_H_
#define _PROCESS_LAUNCHER_H_

#include <utility>

#ifdef WIN32
#  define _CRT_SECURE_NO_WARNINGS 1
#  ifdef UNICODE
#    #undef UNICODE
#  endif
#  include <windows.h>
#else
#  include <unistd.h>
#endif
#include <stdint.h>

/** an alive, spawned process
 *
 * @todo
 *
 * refactor ProcessLauchner and SpawnedProcess into:
 *
 * - ProcessLauncher having ownly the spawn/launch() method and no state
 * - Process as a thin wrapper around 'pid' and operators on it
 * - SpawnedProcess being a Process with stdin/stdout/stderr
 * - a way to declare ownership over the 'pid' (if owned, kill pid in destructor)
 *
 * For now, this mostly exists to make the move-constructor of ProcessLauncher easier to implement.
 */
class SpawnedProcess {
public:
  SpawnedProcess(const char *pcmd_line, const char ** pargs, bool predirect_stderr = true) :
    cmd_line{pcmd_line},
    args{pargs},
#ifdef _WIN32
    child_in_rd{INVALID_HANDLE_VALUE},
    child_in_wr{INVALID_HANDLE_VALUE},
    child_out_rd{INVALID_HANDLE_VALUE},
    child_out_wr{INVALID_HANDLE_VALUE},
    // pi
    // si
#else
    childpid{-1},
    fd_in{-1, -1},
    fd_out{-1, -1},
#endif
    redirect_stderr{predirect_stderr}
  {}

  virtual ~SpawnedProcess() {}

protected:
  const char *cmd_line;
  const char **args;
#ifdef WIN32
  HANDLE child_in_rd;
  HANDLE child_in_wr;
  HANDLE child_out_rd;
  HANDLE child_out_wr;
  PROCESS_INFORMATION pi;
  STARTUPINFO si;
#else
  pid_t childpid;
  int fd_in[2];
  int fd_out[2];
#endif
  bool redirect_stderr;
};


// Launches a process as child of current process and exposes the stdin & stdout of the child process
// (implemented thru pipelines) so the client of this class can read from the child's stdout and write to the child's stdin.
// For usage, see unit tests.
//
class ProcessLauncher : public SpawnedProcess {
public:
  /**
   * Creates a new process and launch it.
   * Argument 'args' must have a last entry that is NULL.
   * If redirect_stderr is true, the child's stderr is redirected to the same stream than child's stdout.
   */
  ProcessLauncher(const char *pcmd_line, const char ** pargs, bool predirect_stderr = true) :
    SpawnedProcess(pcmd_line, pargs, predirect_stderr),
    is_alive{false}
  {}

  // copying a Process results in multiple destructors trying
  // to kill the same alive process. Disable it.
  ProcessLauncher(const ProcessLauncher &) = delete;
  ProcessLauncher operator=(const ProcessLauncher &) = delete;

  ProcessLauncher(ProcessLauncher &&rhs) : SpawnedProcess(rhs), is_alive(std::move(rhs.is_alive)) {
    // make sure destructor on the other object doesn't try to kill
    // the process-id we just moved

    rhs.is_alive = false;
  }

  ~ProcessLauncher() { if (is_alive) close(); }

  /** Launches the child process, and makes pipes available for read/write. */
  void start();

  /**
   * Read up to a 'count' bytes from the stdout of the child process.
   * This method blocks until the amount of bytes is read or specified timeout
   * expires.
   * @param buf already allocated buffer where the read data will be stored.
   * @param count the maximum amount of bytes to read.
   * @param timeout_ms timeout (in milliseconds) for the read to complete
   * @return the real number of bytes read.
   * Returns an shcore::Exception in case of error when reading.
   */
  int read(char *buf, size_t count, unsigned timeout_ms);

  /**
   * Writes several butes into stdin of child process.
   * Returns an shcore::Exception in case of error when writing.
   */
  int write(const char *buf, size_t count);

  /**
   * Kills the child process.
   */
  void kill();

  /**
   * Returns the child process handle.
   * In Linux this needs to be cast to pid_t, in Windows to cast to HANDLE.
   */
  uint64_t get_pid() const;

  /**
   * Wait for the child process to exists and returns its exit code.
   * If the child process is already dead, wait() just returns.
   * Returns the exit code of the process.
   */
  int wait(unsigned int timeout_ms = 1000);

  /**
  * Returns the file descriptor write handle (to write child's stdin).
  * In Linux this needs to be cast to int, in Windows to cast to HANDLE.
  */
  uint64_t get_fd_write() const;

  /**
  * Returns the file descriptor read handle (to read child's stdout).
  * In Linux this needs to be cast to int, in Windows to cast to HANDLE.
  */
  uint64_t get_fd_read() const;

private:
  /**
   * Throws an exception with the specified message, if msg == NULL, the exception's message is specific of the platform error.
   * (errno in Linux / GetLastError in Windows).
   */
  void report_error(const char *msg, const char* prefix = "");
  /** Closes child process */
  void close();

  bool is_alive;
};

#endif // _PROCESS_LAUNCHER_H_
