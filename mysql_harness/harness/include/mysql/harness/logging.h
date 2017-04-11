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

/**
 * @file Logging interface for using and extending the logging
 * subsystem.
 */

#ifndef MYSQL_HARNESS_LOGGING_INCLUDED
#define MYSQL_HARNESS_LOGGING_INCLUDED

#include "filesystem.h"
#include "harness_export.h"

#include <fstream>
#include <mutex>
#include <string>
#include <cstdarg>

#ifndef _WINDOWS
#  include <sys/types.h>
#  include <unistd.h>
#endif

#ifdef _MSC_VER
#  ifdef logger_EXPORTS
/* We are building this library */
#    define LOGGER_API __declspec(dllexport)
#  else
/* We are using this library */
#    define LOGGER_API __declspec(dllimport)
#  endif
#else
#  define LOGGER_API
#endif

#ifdef _WINDOWS
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  define pid_t DWORD
#endif

namespace mysql_harness {

namespace logging {

/**
 * Log level values.
 *
 * Log levels are ordered numerically from most important (lowest
 * value) to least important (highest value).
 */
enum class LogLevel {
  /** Fatal failure. Router usually exits after logging this. */
  kFatal,

  /**
   * Error message. indicate that something is not working properly and
   * actions need to be taken. However, the router continue
   * operating but the particular thread issuing the error message
   * might terminate.
   */
  kError,

  /**
   * Warning message. Indicate a potential problem that could require
   * actions, but does not cause a problem for the continous operation
   * of the router.
   */
  kWarning,

  /**
   * Informational message. Information that can be useful to check
   * the behaviour of the router during normal operation.
   */
  kInfo,

  /**
   * Debug message. Message contain internal details that can be
   * useful for debugging problematic situations, especially regarding
   * the router itself.
   */
  kDebug,

  kNotSet  // Always higher than all other log messages
};

/**
 * Default log level used by the router.
 */
const LogLevel kDefaultLogLevel = LogLevel::kWarning;

/**
 * Log level name for the default log level used by the router.
 */
const char* const kDefaultLogLevelName = "warning";

/**
 * Log record containing information collected by the logging
 * system.
 *
 * The log record is passed to the handlers together with message.
 */
struct Record {
  LogLevel level;
  pid_t process_id;
  time_t created;
  std::string domain;
  std::string message;
};

/**
 * Base class for log message handler.
 *
 * This class is used to implement a log message handler. You need
 * to implement the `do_log` primitive to process the log
 * record. If, for some reason, the implementation is unable to log
 * the record, and exception can be thrown that will be caught by
 * the harness.
 */
class HARNESS_EXPORT Handler {
 public:
  virtual ~Handler() = default;

  void handle(const Record& record);

  void set_level(LogLevel level) { level_ = level; }
  LogLevel get_level() const { return level_; }

 protected:
  std::string format(const Record& record) const;

  explicit Handler(LogLevel level);

 private:
  /**
   * Log message handler primitive.
   *
   * This member function is implemented by subclasses to properly log
   * a record wherever it need to be logged.  If it is not possible to
   * log the message properly, an exception should be thrown and will
   * be caught by the caller.
   *
   * @param record Record containing information about the message.
   */
  virtual void do_log(const Record& record) = 0;

  /**
   * Log level set for the handler.
   */
  LogLevel level_;
};

/**
 * Handler to write to an output stream.
 *
 * @code
 * Logger logger("my_module");
 * ...
 * logger.add_handler(StreamHandler(std::clog));
 * @endcode
 */
class HARNESS_EXPORT StreamHandler : public Handler {
 public:
  explicit StreamHandler(std::ostream& stream,
                         LogLevel level = LogLevel::kNotSet);

 protected:
  std::ostream& stream_;
  std::mutex stream_mutex_;

 private:
  void do_log(const Record& record) override;
};

/**
 * Handler that writes to a file.
 *
 * @code
 * Logger logger("my_module");
 * ...
 * logger.add_handler(FileHandler("/var/log/router.log"));
 * @endcode
 */
class HARNESS_EXPORT FileHandler : public StreamHandler {
 public:
  explicit FileHandler(const Path& path, LogLevel level = LogLevel::kNotSet);
  ~FileHandler();

 private:
  std::ofstream fstream_;
};

/** Set log level for all registered loggers. */
HARNESS_EXPORT
void set_log_level(LogLevel level);

/** Set log level for the named logger. */
HARNESS_EXPORT
void set_log_level(const char* name, LogLevel level);

/**
 * Register handler for all plugins.
 *
 * This will register a handler for all plugins that have been
 * registered with the logging subsystem (normally all plugins that
 * have been loaded by `Loader`).
 *
 * @param handler Shared pointer to dynamically allocated handler.
 *
 * For example, to register a custom handler from a plugin, you would
 * do the following:
 *
 * @code
 * void init() {
 *   ...
 *   register_handler(std::make_shared<MyHandler>(...));
 *   ...
 * }
 * @endcode
 */
HARNESS_EXPORT
void register_handler(std::shared_ptr<Handler> handler);

/**
 * Unregister a handler.
 *
 * This will unregister a previously registered handler.
 *
 * @param handler Shared pointer to a previously allocated handler.
 */
HARNESS_EXPORT
void unregister_handler(std::shared_ptr<Handler> handler);

/**
 * Log message for the domain.
 *
 * This will log an error, warning, informational, or debug message
 * for the given domain. The domain have to be be registered before
 * anything is being logged. The `Loader` uses the plugin name as the
 * domain name, so normally you should provide the plugin name as the
 * first argument to this function.
 *
 * @param name Domain name to use when logging message.
 *
 * @param fmt `printf`-style format string, with arguments following.
 */
/** @{ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Pre-processor symbol containing the name of the log domain. If not
 * defined explicitly when compiling, it will be an empty string, which
 * means that it logs to the top log domain.
 */

#ifndef MYSQL_ROUTER_LOG_DOMAIN
#define MYSQL_ROUTER_LOG_DOMAIN ""
#endif

/*
 * Declare the implementation log functions and define inline
 * functions that pick up the log domain defined for the module.
 * These functions are namespace-aware.
 */

inline void log_error(const char* fmt, ...) {
  extern void _vlog_error(const char* name, const char *fmt, va_list ap);
  va_list ap;
  va_start(ap, fmt);
  _vlog_error(MYSQL_ROUTER_LOG_DOMAIN, fmt, ap);
  va_end(ap);
}

inline void log_warning(const char* fmt, ...) {
  extern void _vlog_warning(const char* name, const char *fmt, va_list ap);
  va_list ap;
  va_start(ap, fmt);
  _vlog_warning(MYSQL_ROUTER_LOG_DOMAIN, fmt, ap);
  va_end(ap);
}

inline void log_info(const char* fmt, ...) {
  extern void _vlog_info(const char* name, const char *fmt, va_list ap);
  va_list ap;
  va_start(ap, fmt);
  _vlog_info(MYSQL_ROUTER_LOG_DOMAIN, fmt, ap);
  va_end(ap);
}

inline void log_debug(const char* fmt, ...) {
  extern void _vlog_debug(const char* name, const char *fmt, va_list ap);
  va_list ap;
  va_start(ap, fmt);
  _vlog_debug(MYSQL_ROUTER_LOG_DOMAIN, fmt, ap);
  va_end(ap);
}

/** @} */

#ifdef __cplusplus
}
#endif

}  // namespace logging

}  // namespace mysql_harness

/**
 * convenience macro to avoid common boilerplate
 */
#define IMPORT_LOG_FUNCTIONS() \
using mysql_harness::logging::log_error;    \
using mysql_harness::logging::log_warning;  \
using mysql_harness::logging::log_info;     \
using mysql_harness::logging::log_debug;

#endif // MYSQL_HARNESS_LOGGING_INCLUDED
