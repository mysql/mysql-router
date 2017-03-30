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

#ifndef MYSQL_HARNESS_LOGGER_INCLUDED
#define MYSQL_HARNESS_LOGGER_INCLUDED

#include "mysql/harness/plugin.h"
#include "mysql/harness/logging.h"

#include "mysql/harness/filesystem.h"

#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <list>

namespace mysql_harness {

namespace logging {

/**
 * Logger class.
 *
 * The logger class handle the logging for one or more logging
 * handlers. Each logger class instance keep state for logging for one
 * module or subsystem. You can add handlers to a logger which will
 * then be used for all logging to that subsystem.
 */
class Logger {
 public:
  explicit Logger(const std::string& subsystem,
                  LogLevel level = kDefaultLogLevel);

  void add_handler(std::shared_ptr<Handler>);
  void remove_handler(std::shared_ptr<Handler> handler);
  void handle(const Record& record);

  void set_level(LogLevel level) { level_ = level; }
  LogLevel get_level() const { return level_; }
  const std::string& get_name() const { return name_; }

 private:
  std::string name_;
  LogLevel level_;
  std::vector<std::shared_ptr<Handler>> handlers_;
};

}  // namespace logging

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_LOGGER_INCLUDED */
