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

#ifndef MYSQL_HARNESS_LOGGER_REGISTRY_INCLUDED
#define MYSQL_HARNESS_LOGGER_REGISTRY_INCLUDED

#include "mysql/harness/logging.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/config_parser.h"
#include "harness_export.h"

namespace mysql_harness {

namespace logging {

/**
 * Return filename of file into which application logs.
 *
 * @note If application is logging to console, it will return
 * an empty string.
 */
HARNESS_EXPORT
const mysql_harness::Path& get_log_file();

/**
 * Create a logger in the internal registry.
 *
 * @param name Module name for the logger.
 * @param level Log level for logger.
 */
HARNESS_EXPORT
void create_logger(const std::string& name,
                   LogLevel level = LogLevel::kNotSet);

/**
 * Remove a named logger from the internal registry.
 *
 * @param name Module name for the logger.
 */
HARNESS_EXPORT
void remove_logger(const std::string& name);

/**
 * Initialize logging facility
 *
 * Initializes logging facility by registering all loggers/handlers and
 * opening appropriate stream for logging. If `logging_folder` is empty,
 * messages will be logged to console, otherwise, logfile will be used and
 * its path and filename will be derived from `program` and `logging_folder`
 * parameters.
 *
 * @param program Name of the main program (Router)
 * @param logging_folder logging_folder provided in configuration file
 * @param config Configuration items from configuration file
 * @param modules List of plugin names loaded
 */
HARNESS_EXPORT
void setup(const std::string& program,
           const std::string& logging_folder,
           const mysql_harness::Config& config,
           const std::list<std::string>& modules);

/**
 * Deinitialize logging facility
 */
HARNESS_EXPORT
void teardown();

/**
 * Get the logger names from the internal registry.
 */
HARNESS_EXPORT
std::list<std::string> get_logger_names();

}  // namespace logging

} // namespace mysql_harness

#endif /* MYSQL_HARNESS_LOGGER_REGISTRY_INCLUDED */
