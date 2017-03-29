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

#include "mysql/harness/filesystem.h"
#include "mysql/harness/config_parser.h"

namespace mysql_harness {

namespace logging {

const mysql_harness::Path& get_log_file();

/**
 * Create a logger in the internal registry.
 *
 * @param name Module name for the logger.
 */
void create_logger(const std::string& name);

/**
 * Remove a named logger from the internal registry.
 *
 * @param name Module name for the logger.
 */
void remove_logger(const std::string& name);


void setup(const std::string& program,
           const std::string& logging_folder,
           const mysql_harness::Config& config,
           const std::list<std::string>& modules);

void teardown();

/**
 * Get the logger names from the internal registry.
 */
std::list<std::string> get_logger_names();

}  // namespace logging

} // namespace mysql_harness

#endif /* MYSQL_HARNESS_LOGGER_REGISTRY_INCLUDED */
