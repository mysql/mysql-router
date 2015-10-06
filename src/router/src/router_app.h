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

#ifndef ROUTER_MYSQL_ROUTER_INCLUDED
#define ROUTER_MYSQL_ROUTER_INCLUDED

/** @file
 * @brief Defining the main class MySQLRouter
 *
 * This file defines the main class `MySQLRouter`.
 *
 */

#include "arg_handler.h"
#include "config.h"

#include <cassert>
#include <cstdint>
#include <tuple>
#include <vector>

#include "loader.h"

using std::string;
using std::tuple;
using std::make_tuple;
using std::vector;

static const size_t kHelpScreenWidth = 72;
static const size_t kHelpScreenIndent = 8;

/** @class MySQLRouter
 *  @brief Manage the MySQL Router application.
 *
 *  The class MySQLRouter manages the MySQL Router application. It handles the
 *  command arguments, finds valid configuration files, and starts all plugins.
 *
 *  Since MySQL Router requires at least 1 configuration file to be available
 *  for reading, if no default configuration file location could be read and no
 *  explicit location was given, the application exits.
 *
 *  The class depends on MySQL Harness to, among other things, load the
 *  configuration and initalize all request plugins.
 *
 *  Example usage:
 *
 *     int main(int argc, char** argv) {
 *       MySQLRouter router(argc, argv);
 *       router.start();
 *     }
 *
 */
class MySQLRouter {
public:
  /** @brief Default constructor
   *
   * Default constructor of MySQL Router which will not initialize.
   *
   * Usage:
   *
   *     MySQLRouter router;
   *     router.start();
   *
   */
  MySQLRouter() : can_start_(false), showing_info_(false) {};


  /** @brief Constructor with command line arguments as vector
   *
   * Constructor of MySQL Router which will start with the given command
   * line arguments as a vector of strings.
   *
   * Example usage:
   *
   *     MySQLRouter router(Path(argv[0]).dirname(),
   *                        vector<string>({argv + 1, argv + argc}));
   *     router.start();
   *
   * @param origin Directory where executable is located
   * @param arguments a vector of strings
   */
  MySQLRouter(const Path& origin, const vector<string>& arguments);

  /** @brief Constructor with command line arguments
   *
   * Constructor of MySQL Router which will start with the given command
   * line arguments given as the arguments argc and argv. Typically, argc
   * and argv are passed on from the global main function.
   *
   * Example usage:
   *
   *     int main(int argc, char** argv) {
   *       MySQLRouter router(argc, argv);
   *       router.start();
   *     }
   *
   * @param argc number of arguments
   * @param argv pointer to first command line argument
   */
  MySQLRouter(const int argc, char** argv);

  // Information member function
  string get_package_name() noexcept;

  /** @brief Returns the MySQL Router version as string
   *
   * Returns the MySQL Router as a string. The string is a concatenation
   * of version's major, minor and patch parts, for example `1.0.2`.
   *
   * @return string containing the version
   */
  string get_version() noexcept;

  /** @brief Returns string version details.
   *
   * Returns string with name and version details, including:
   *
   * * name of the application,
   * * version,
   * * platform and architecture,
   * * edition,
   * * and a special part-of-clause.
   *
   * The architecture is either 32-bit or 64-bit. Edition is usually used to
   * denote whether release is either GPLv2 license or commercial.
   *
   * The part-of clause is used to show which product family MySQL Router
   * belongs to.
   *
   * @devnote
   * Most information can be defined while configuring using CMake and will
   * become available through the router_config.h file (generated from
   * router_config.h.in).
   * @enddevnote
   *
   * @return a string containing version details
   */
  string get_version_line() noexcept;

  /** @brief Prepares a command line option
   *
   * Prepares command line options for the MySQL Router `mysqlrouter` application.
   *
   * @internal
   * Currently, to add options to the command line, you need to add it to the
   * `prepare_command_options`-method using `CmdArgHandler::add_option()`.
   * @endinternal
   */
  void prepare_command_options() noexcept;

  /** @brief Starts the MySQL Router application
   *
   * Starts the MySQL Router application, reading the configuration file(s) and
   * loading and starting all plugins.
   *
   * Example:
   *
   *     MySQLRouter router;
   *     router.start();
   *
   * Throws std::runtime_error on configuration or plugin errors.
   *
   * @devnote
   * We are using MySQL Harness to load and start the plugins. We give Harness
   * a configuration files and it will parse it. Not that at this moment, Harness
   * only accept one configuration file.
   * @enddevnote
   */
  void start();

private:

  /** @brief Initializes the MySQL Router application
   *
   * Initialized the MySQL Router application by
   *
   * * setting the default configuration files,
   * * loading the command line options,
   * * processing the given command line arguments,
   * * and finding all the usable configuration files.
   *
   * The command line options are passed using the `arguments`
   * argument. It should be a vector of strings. For example, to start
   * MySQL Router using the `main()` functions `argc` and `argv`
   * arguments:
   *
   *     MySQLRouter router(vector<string>({argv + 1, argv + argc}));
   *     router.start();
   *
   * @devnote
   * We do not need the first command line argument, argv[0] since we do not
   * use it.
   * @enddevnote
   *
   * @param arguments command line arguments as vector of strings
   */
  void init(const vector<string>& arguments);

  /** @brief Finds all valid configuration files
   *
   * Finds all valid configuration files from the list of default
   * configuration file locations.
   *
   * An exception of type `std::runtime_error` is thrown when no valid
   * configuration file was found.
   *
   * @return returns a list of valid configuration file locations
   *
   */
  vector<string> check_config_files();

  /** @brief Shows the help screen on the console
   *
   * Shows the help screen on the console including
   *
   * * copyright and  trademark notice,
   * * command line usage,
   * * default configuration file locations,
   * * and options with their descriptions.
   *
   * Users would use the command line option `--help`:
   *
   *     shell> mysqlrouter --help
   */
  void show_help() noexcept;

  /** @brief Shows command line usage and option description
   *
   * Shows command line usage and all available options together with their description.
   * It is possible to prevent the option listing by setting the argument `include_options`
   * to `false`.
   *
   * @devnote
   * This method is used by the `MySQLRouter::show_help()`. We keep a separate method so we could potentionally
   * show the usage in other places or using different option combinations, for example after an
   * error.
   * @enddevnote
   *
   * @param include_options bool whether we show the options and descriptions
   */
  void show_usage(bool include_options) noexcept;

  /* @overload */
  void show_usage() noexcept;

  /** @brief Sets default configuration file locations
   *
   *  Sets the default configuration file locations based on information
   *  found in the locations argument. The locations should be provided
   *  as a semicolon separated list.
   *
   *  The previous loaded locations are first removed. If not new locations
   *  were provider (if locations argument is empty), then no configuration
   *  files will be available.
   *
   *  Locations can include environment variable placeholders. These placeholders
   *  are replaced using the provided name. For example, user Jane executing
   *  MySQL Router:
   *
   *      /opt/ENV{USER}/etc    becomes   /opt/jane/etc
   *
   *  If the environment variable is not available, for example if MYSQL_ROUTER_HOME
   *  was not set before starting MySQL Router, every location using this
   *  environment variable will be ignored.
   *
   *  @param locations a char* with semicolon separated file locations
   */
  void set_default_config_files(const char *locations) noexcept;

  /** @brief Tuple describing the MySQL Router version, with major, minor and patch level **/
  tuple<const uint8_t, const uint8_t, const uint8_t> version_;

  /** @brief Vector with default configuration file locations as strings **/
  std::vector<string> default_config_files_;
  /** @brief Vector with extra configuration file locations as strings **/
  std::vector<string> extra_config_files_;
  /** @brief Vector with configuration files passed through command line arguments **/
  vector<string> config_files_;
  /** @brief PID file location **/
  string pid_file_path_;
  /** @brief Vector with available and usable configuration files
   *
   * @devnote
   * config_files_ is a vector but it should only contain 1 element since the command line
   * option for passing these configuration files can only be used once. We use a vector
   * to make it ourselves easier when looping through all types of configuration files.
   * @enddevnote
   */
  vector<string> available_config_files_;
  /** @brief CmdArgHandler object handling command line arguments **/
  CmdArgHandler arg_handler_;
  /** @brief Harness loader **/
  std::unique_ptr<Loader> loader_;
  /** @brief Whether the MySQLRouter can start or not **/
  bool can_start_;
  /** @brief Whether we are showing information on command line, for example, using --help or --version **/
  bool showing_info_;

  /**
   * Path to origin of executable.
   *
   * This variable contain the directory that the executable is
   * running from.
   */
  Path origin_;
};

#endif // ROUTER_MYSQL_ROUTER_INCLUDED
