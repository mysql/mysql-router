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
#  define WIN32_LEAN_AND_MEAN
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

// TODO one day we'll improve this and move it to a common spot
#define harness_assert(COND) if (!(COND)) abort();

std::mutex g_loggers_mutex;
static std::map<std::string, Logger> g_loggers;

static Path g_log_file;

static std::map<std::string, LogLevel> g_levels{
  {"fatal", LogLevel::kFatal},
  {"error", LogLevel::kError},
  {"warning", LogLevel::kWarning},
  {"info", LogLevel::kInfo},
  {"debug", LogLevel::kDebug},
};

namespace mysql_harness {

namespace logging {

// this is the MYSQL_ROUTER_LOG_DOMAIN of our main binary (Router)
extern const char kMainAppLogDomain[];

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
    auto level = g_levels.at(level_name);
    for (auto& module : modules)
      create_logger(module, level);
  } catch (std::out_of_range& exc) {
    std::stringstream buffer;

    buffer << "Log level '" << level_name
           << "' is not valid. Valid values are: ";

    // Print the entries using a serial comma
    std::vector<std::string> alternatives;
    for (auto& entry : g_levels)
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

  // ensure that we have at least 1 logger registered: the main app logger
  harness_assert(g_loggers.count(kMainAppLogDomain));
}

void teardown() {
  std::lock_guard<std::mutex> lock(g_loggers_mutex);
  g_loggers.clear();
}

}  // namespace logging

}  // namespace mysql_harness


////////////////////////////////////////////////////////////////
// Logging functions for use by plugins.

// We want to hide log_message(), because instead we want plugins to call
// log_error(), log_warning(), etc. However, those functions must be inline
// and are therefore defined in the header file - which means log_message()
// must have external linkage. So to solve this visibility conflict, we declare
// log_message() locally, inside of log_error(), log_warning(), etc.
//
// Normally, this would only leave us with having to define log_message() here.
// However, since we are building a DLL/DSO with this file, and since VS only
// allows __declspec(dllimport/dllexport) in function declarations, we must
// provide both declaration and definition.
extern "C" void LOGGER_API log_message(LogLevel level, const char* module, const char* fmt, va_list ap);

extern "C" void log_message(LogLevel level, const char* module, const char* fmt, va_list ap) {
  harness_assert(level <= LogLevel::kDebug);

  using mysql_harness::logging::kMainAppLogDomain;

  // get timestamp
  time_t now;
  time(&now);

  // Find the logger for the module
  // NOTE that we copy the logger. Even if some other thread removes this
  //      logger from g_loggers, our call will still be valid, as our logger
  //      object secured the necessary resources (via shared_ptr).
  Logger logger;
  try {
    std::lock_guard<std::mutex> lock(g_loggers_mutex);
    logger = g_loggers.at(module);
  } catch (std::out_of_range& exc) {
    // Logger is not registered for this module (log domain), so log as main
    // application domain instead
    harness_assert(g_loggers.count(kMainAppLogDomain));
    logger = g_loggers[kMainAppLogDomain];

    // Complain that we're logging this elsewhere
    char msg[mysql_harness::logging::kLogMessageMaxSize];
    snprintf(msg, sizeof(msg),
             "Module '%s' not registered with logger - "
             "logging the following message as '%s' instead",
             module, kMainAppLogDomain);
    logger.handle({LogLevel::kError, getpid(), now, kMainAppLogDomain, msg});

    // And switch log domain to main application domain for the original
    // log message
    module = kMainAppLogDomain;
  }

  // Build the message
  char message[mysql_harness::logging::kLogMessageMaxSize];
  vsnprintf(message, sizeof(message), fmt, ap);

  // Build the record for the handler.
  Record record{level, getpid(), now, module, message};

  // Pass the record to the correct logger. The record should be
  // passed to only one logger since otherwise the handler can get
  // multiple calls, resulting in multiple log records.
  logger.handle(record);
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

}  // namespace logging

}  // namespace mysql_harness
