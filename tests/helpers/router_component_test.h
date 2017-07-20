/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "process_launcher.h"
#include "router_test_helpers.h"

#include <cstring>
#include <sstream>
#include <map>
#include <streambuf>
#include <vector>
#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#endif

using mysql_harness::Path;

/** @class UniqueID
 *
 * Helper class allowing mechanism to retrieve system-level unique identifier.
 * Compatible with mysql-test MTR, see mysql-test/lib/mtr_unique.pm for details
 *
 **/
class UniqueId {
 public:
  UniqueId(unsigned start_from, unsigned range);
  UniqueId(UniqueId&& other);
  ~UniqueId();

  UniqueId(const UniqueId&) = delete;
  UniqueId& operator=(const UniqueId&) = delete;

  unsigned get() const {
    return id_;
  }

 private:
   bool lock_file(const std::string& file_name);
   std::string get_lock_file_dir() const;

  unsigned id_;
#ifndef _WIN32
  int lock_file_fd_;
#else
  HANDLE lock_file_fd_;
#endif
  std::string lock_file_name_;
};

/** @class TcpPortPool
 *
 * Helper class allowing mechanism to retrieve pool of the system-level unique TCP port numbers.
 * Compatible with mysql-test MTR, see mysql-test/lib/mtr_unique.pm for details.
 *
 **/
class TcpPortPool {
public:
  TcpPortPool(unsigned start_from = 1, unsigned range = 300):
    unique_id_(start_from, range) {}

  TcpPortPool(const TcpPortPool&) = delete;
  TcpPortPool& operator=(const TcpPortPool&) = delete;
  TcpPortPool(TcpPortPool&& other) = default;

  unsigned get_next_available();
private:
  UniqueId unique_id_;
  unsigned number_of_ids_used_{0};
  static const int kMaxPort{10};
};

/** @brief maximum number of parameters that can be passed to the launched process */
const size_t MAX_PARAMS{30};

/** @class RouterComponentTest
 *
 * Base class for the MySQLRouter component-like tests.
 * Enables creating processes, intercepting their output, writing to input, etc.
 *
 **/
class RouterComponentTest {
 protected:

  RouterComponentTest();
  virtual ~RouterComponentTest() = default;

  /** @class CommandHandle
   *
   * Object of this class gets return from launch_* method and can be
   * use to manipulate launched process (get the output, exit code,
   * inject input, etc.)
   *
   **/
  class CommandHandle {
   public:
    /** @brief Checks if the process wrote the specified string to its output.
     *
     * @param str         Expected output string
     * @param regex       True if str is a regex pattern
     * @param timeout_ms  Timeout in milliseconds, to wait for the output
     * @return Returns bool flag indicating if the specified string appeared
     *                 in the process' output.
     */
    bool expect_output(const std::string& str,
                       bool regex = false,
                       unsigned timeout_ms = 1000);

    /** @brief Returns the full output that was produced the process till moment
     *         of calling this method.
     */
    std::string get_full_output() {
      while (read_output(0)) {}
      return execute_output_raw_;
    }

    /** @brief Register the response that should be written to the process' input descriptor
     *         when the given string appears on it output while executing expect_output().
     *
     * @param query     string that should trigger writing the response
     * @param response  string that should get written
     */
    void register_response(const std::string &query,
                           const std::string &response) {
      output_responses_[query] = response;
    }

    /** @brief Returns the exit code of the process.
     *
     *  Must always be called after wait_for_exit(),
     *  otherwise it throws runtime_error
     *
     * @returns exit code of the process
     */
    int exit_code() {
      if (!exit_code_set_) {
        throw std::runtime_error("RouterComponentTest::Command_handle: exit_code() called without wait_for_exit()!");
      }
      return exit_code_;
    }

    /** @brief Waits for the process to exit.
     *
     *  If the process did not finish yet waits the given number of milliseconds.
     *  If the timeout expired it throws runtime_error.
     *  In case of failure it throws system_error.
     *
     * @param timeout_ms maximum amount of time to wait for the process to finish
     *
     * @returns exit code of the process
     */
    int wait_for_exit(unsigned timeout_ms = 1000);

   private:
    CommandHandle(const std::string &app_cmd,
                 const char **args,
                 bool include_stderr):
       launcher_(app_cmd.c_str(), args, include_stderr) {
     launcher_.start();
    }

    bool output_contains(const std::string& str,
                         bool regex = false) const;

    bool read_output(unsigned timeout_ms);
    void handle_output(const std::string &line);

    ProcessLauncher launcher_; // <- this guy's destructor takes care of
                               // killing the spawned process
    std::string execute_output_raw_;
    std::map<std::string, std::string> output_responses_;
    int exit_code_;
    bool exit_code_set_{false};

    friend class RouterComponentTest;
  };

  /** @brief Gtest class SetUp, prepares the testcase.
   */
  virtual void SetUp();

  /** @brief Launches the MySQLRouter process.
   *
   * @param   params string containing command line parameters to pass to process
   * @param   catch_stderr bool flag indicating if the process' error output stream
   *                       should be included in the output caught from the process
   * @param   with_sudo    bool flag indicating if the process' should be execute with
   *                       sudo priviledges
   *
   * @returns handle to the launched proccess
   */
  CommandHandle launch_router(const std::string &params,
                     bool catch_stderr = true,
                     bool with_sudo = false) const;

  /** @brief Launches the MySQLServerMock process.
   *
   * @param   json_file path to the json file containing expected queries definitions
   * @param   port      number of the port where the mock server will accept the
   *                    client connections
   *
   * @returns handle to the launched proccess
   */
  CommandHandle launch_mysql_server_mock(const std::string& json_file,
                                         unsigned port) const;

  /** @brief Removes non-empty directory recursively.
   *
   * @param dir name of the directory to remove
   *
   * @returns 0 on success, error code on failure
   */
  static int purge_dir(const std::string& dir);

  /** @brief Creates a temporary directory with partially-random name and returns
   * its path.
   *
   * @note This is a convenience proxy function to mysql_harness::get_tmp_dir(),
   * see documentation there for more details.
   *
   * @param name name to be used as a directory name prefix
   *
   * @return path to the created directory
   *
   * @throws std::runtime_error if operation failed
   */
  static std::string get_tmp_dir(const std::string &name = "router");

  /** @brief Probes if the selected TCP port is accepting the connections.
   *
   * @param port          TCP port number to check
   * @param timeout_msec  maximum timeout to wait for the port
   * @param hostname      name/IP address of the network host to check
   *
   * @returns true if the selected port accepts connections, false otherwise
   */
  bool wait_for_port_ready(unsigned port, unsigned timeout_msec,
                           const std::string &hostname = "127.0.0.1") const;

  /** @brief Gets path to the directory containing testing data
   *         (conf files, json files).
   */
  const Path &get_data_dir() const {
    return data_dir_;
  }

  /** @brief returns a map with default [DEFAULT] section parameters
   *
   * @return default parameters for [DEFAULT] section
   */
  std::map<std::string, std::string> get_DEFAULT_defaults() const;

  std::string create_config_file(const std::string &content = "",
                                 const std::string &directory = get_tmp_dir("conf"),
                                 const std::string &name = "mysqlrouter.conf") const;

  void set_origin(const Path &origin) {
    origin_dir_ = origin;
  }

 private:
  CommandHandle launch_command(const std::string &command,
                               const std::string &params,
                               bool catch_stderr) const;

  void get_params(const std::string &command,
                  const std::vector<std::string> &params_vec,
                  const char* out_params[MAX_PARAMS]) const;

  Path data_dir_;
  Path origin_dir_;
  Path stage_dir_;
  Path plugin_dir_;
  Path mysqlrouter_exec_;
  Path mysqlserver_mock_exec_;
};
