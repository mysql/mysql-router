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

#include "config_parser.h"
#include "logger.h"
#include "logging_registry.h"
#include "utilities.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <sstream>
#include <cstdarg>

using mysql_harness::Path;
using mysql_harness::logging::LogLevel;
using mysql_harness::logging::Logger;
using mysql_harness::logging::Record;
using mysql_harness::utility::serial_comma;

static std::map<std::string, Logger> g_loggers;

static Path g_log_file;

static std::map<std::string, LogLevel> levels{
  {"fatal", LogLevel::kFatal},
  {"error", LogLevel::kError},
  {"warning", LogLevel::kWarning},
  {"info", LogLevel::kInfo},
  {"debug", LogLevel::kDebug},
};

namespace mysql_harness {

namespace logging {

void create_logger(const std::string& name) {
  auto result = g_loggers.emplace(name, Logger(name));
  if (result.second == false)
    throw std::logic_error("Duplicate logger for section '" + name + "'");
}

void remove_logger(const std::string& name) {
  if (g_loggers.erase(name) == 0)
    throw std::logic_error("Removing non-existant logger '" + name + "'");
}

std::list<std::string> get_logger_names() {
  std::list<std::string> result;
  for (auto&& entry : g_loggers)
    result.push_back(entry.second.get_name());
  return result;
}

void setup(const std::string& program,
           const std::string& logging_folder,
           const Config& config,
           const std::list<std::string>& modules) {
  // Before initializing, but after all modules are loaded, we set up
  // the logging subsystem and create one logger for each loaded
  // plugin.
  for (auto&& module : modules)
    create_logger(module);

  if (!config.has_default("log_level")) {
    // If there is no log level defined, we set it to the default.
    set_log_level(LogLevel::kInfo);
  } else {
    // We set the log level for all modules to whatever is defined in
    // the default section.
    auto level_value = config.get_default("log_level");
    std::transform(level_value.begin(), level_value.end(),
                   level_value.begin(), ::toupper);

    try {
      set_log_level(levels.at(level_value));
    } catch (std::out_of_range& exc) {
      std::stringstream buffer;

      buffer << "Log level '" << level_value
             << "' is not valid. Valid values are: ";

      // Print the entries using a serial comma
      std::vector<std::string> alternatives;
      for (auto&& entry : levels)
        alternatives.push_back(entry.first);
      serial_comma(buffer, alternatives.begin(), alternatives.end());

      throw std::invalid_argument(buffer.str());
    }
  }

  if (logging_folder.empty()) {
    // Register the console as the handler if there is no log file.
    register_handler(std::make_shared<StreamHandler>(std::cerr));
  } else {
    // Register a file log handler
    g_log_file = Path::make_path(logging_folder, program, "log");
    register_handler(std::make_shared<FileHandler>(g_log_file));
  }
}

void teardown() {
  for (auto&& entry : g_loggers)
    remove_logger(entry.second.get_name());
}

}  // namespace logging

}  // namespace mysql_harness


////////////////////////////////////////////////////////////////
// Logging functions for use by plugins.

namespace {

template <LogLevel level>
void log_message(const char* module, const char* fmt, va_list ap) {
  assert(level <= LogLevel::kDebug);

  try {
    // Find the logger for the module
    auto logger = g_loggers.at(module);

    // Build the message
    char message[256];
    vsnprintf(message, sizeof(message), fmt, ap);

    // Build the record for the handler.
    time_t now;
    time(&now);
    Record record{level, getpid(), now, module, message};

    // Pass the record to the correct logger. The record should be
    // passed to only one logger since otherwise the handler can get
    // multiple calls, resulting in multiple log records.
    logger.handle(record);
  } catch (std::out_of_range& exc) {
    throw std::logic_error("Module '" + std::string(module) +
                           "' not registered");
  }
}

}

namespace mysql_harness {

namespace logging {

void set_log_level(LogLevel level) {
  for (auto&& entry : g_loggers)
    entry.second.set_level(level);
}

const Path& get_log_file() {
  return g_log_file;
}

void register_handler(std::shared_ptr<Handler> handler) {
  for (auto&& entry : g_loggers)
    entry.second.add_handler(handler);
}

void log_error(const char* module, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message<LogLevel::kError>(module, fmt, args);
  va_end(args);
}


void log_warning(const char* module, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message<LogLevel::kWarning>(module, fmt, args);
  va_end(args);
}


void log_info(const char* module, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message<LogLevel::kInfo>(module, fmt, args);
  va_end(args);
}


void log_debug(const char* module, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message<LogLevel::kDebug>(module, fmt, args);
  va_end(args);
}

}  // namespace logging

}  // namespace mysql_harness
