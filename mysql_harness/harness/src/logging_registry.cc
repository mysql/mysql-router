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

#ifdef _WINDOWS
#  include <windows.h>
#  define getpid GetCurrentProcessId
#endif

#include "logging_registry.h"

#include "mysql/harness/config_parser.h"

#include "utilities.h"
#include "logger.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <sstream>
#include <cstdarg>
#include <mutex>


using mysql_harness::Path;
using mysql_harness::logging::LogLevel;
using mysql_harness::logging::Logger;
using mysql_harness::logging::Record;
using mysql_harness::utility::serial_comma;

std::mutex g_loggers_mutex;
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

void create_logger(const std::string& name, LogLevel level) {
  std::lock_guard<std::mutex> lock(g_loggers_mutex);
  auto result = g_loggers.emplace(name, Logger(name, level));
  if (result.second == false)
    throw std::logic_error("Duplicate logger for section '" + name + "'");
}

void remove_logger(const std::string& name) {
  std::lock_guard<std::mutex> lock(g_loggers_mutex);
  if (g_loggers.erase(name) == 0)
    throw std::logic_error("Removing non-existant logger '" + name + "'");
}

std::list<std::string> get_logger_names() {
  std::lock_guard<std::mutex> lock(g_loggers_mutex);
  std::list<std::string> result;
  for (auto& entry : g_loggers)
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

  // We get the default log level from the configuration.
  auto level_name = config.get_default("log_level");
  std::transform(level_name.begin(), level_name.end(),
                 level_name.begin(), ::tolower);
  try {
    // Create a logger for each module in the logging registry.
    auto level = levels.at(level_name);
    for (auto& module : modules)
      create_logger(module, level);
  } catch (std::out_of_range& exc) {
    std::stringstream buffer;

    buffer << "Log level '" << level_name
           << "' is not valid. Valid values are: ";

    // Print the entries using a serial comma
    std::vector<std::string> alternatives;
    for (auto& entry : levels)
      alternatives.push_back(entry.first);
    serial_comma(buffer, alternatives.begin(), alternatives.end());
    throw std::invalid_argument(buffer.str());
  }

  // Register the console as the handler if the logging folder is
  // undefined. Otherwise, register a file handler.
  if (logging_folder.empty()) {
    register_handler(std::make_shared<StreamHandler>(std::cerr));
  } else {
    g_log_file = Path::make_path(logging_folder, program, "log");
    register_handler(std::make_shared<FileHandler>(g_log_file));
  }
}

void teardown() {
  std::lock_guard<std::mutex> lock(g_loggers_mutex);
  g_loggers.clear();
}

}  // namespace logging

}  // namespace mysql_harness


////////////////////////////////////////////////////////////////
// Logging functions for use by plugins.

namespace {

void log_message(LogLevel level, const char* module, const char* fmt, va_list ap) {
  assert(level <= LogLevel::kDebug);

// FIXME mod Mats' code to make it unittestable
  if (!module[0])
    return;

  try {
// FIXME: we hold this lock very very very long here.
    std::lock_guard<std::mutex> lock(g_loggers_mutex);
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
// FIXME think about what to do with this
// pro leaving it: unless user sets log level to warning or error, you'll find out right away if you have this problem
// pro erasing it: - log_error() is frequently called as part of error handling.  When it throws, we introduce another problem.  It's sort of like throwing in a destructor.
//                 - how do you properly code your application to catch this exception?  try-catch inside of catch (error handling) and everywhere else?  global try-catch in main()?
//                 - when this throws, it will probably down the router.  Good idea?
#if 0
    throw std::logic_error("Module '" + std::string(module) +
                           "' not registered");
#endif
  }
}

}

namespace mysql_harness {

namespace logging {

void set_log_level(LogLevel level) {
  std::lock_guard<std::mutex> lock(g_loggers_mutex);
  for (auto& entry : g_loggers)
    entry.second.set_level(level);
}

const Path& get_log_file() {
  return g_log_file;
}

void register_handler(std::shared_ptr<Handler> handler) {
  std::lock_guard<std::mutex> lock(g_loggers_mutex);
  for (auto& entry : g_loggers)
    entry.second.add_handler(handler);
}

void unregister_handler(std::shared_ptr<Handler> handler) {
  std::lock_guard<std::mutex> lock(g_loggers_mutex);
  for (auto& entry : g_loggers)
    entry.second.remove_handler(handler);
}


extern "C" void LOGGER_API _vlog_error(const char* module, const char *fmt, va_list args);
extern "C" void LOGGER_API _vlog_warning(const char* module, const char *fmt, va_list args);
extern "C" void LOGGER_API _vlog_info(const char* module, const char *fmt, va_list args);
extern "C" void LOGGER_API _vlog_debug(const char* module, const char *fmt, va_list args);

extern "C" void
_vlog_error(const char* module, const char *fmt, va_list args) {
  log_message(LogLevel::kError, module, fmt, args);
}


extern "C" void
_vlog_warning(const char* module, const char *fmt, va_list args) {
  log_message(LogLevel::kWarning, module, fmt, args);
}


extern "C" void
_vlog_info(const char* module, const char *fmt, va_list args) {
  log_message(LogLevel::kInfo, module, fmt, args);
}


extern "C" void
_vlog_debug(const char* module, const char *fmt, va_list args) {
  log_message(LogLevel::kDebug, module, fmt, args);
}

}  // namespace logging

}  // namespace mysql_harness
