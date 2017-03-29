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

#include "filesystem.h"

#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

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

namespace mysql_harness {

namespace logging {

enum class LogLevel {
  kFatal,  // Fatal errors: logged before the harness terminate
  kError,
  kWarning,
  kInfo,
  kDebug
};


/**
 * Log record containing information collected by the logging
 * system.
 *
 * The log record is passed to the handlers together with the format
 * string and the arguments.
 */
struct Record {
  LogLevel level;
  pid_t process_id;
  time_t created;
  std::string module;
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
class Handler {
 public:
  virtual ~Handler() = default;

  void handle(const Record& record);

 protected:
  // ??? Does this reall have to be a member function and does it ???
  // ??? belong to the handler ???
  std::string format(const Record& record) const;

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
};


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
                  LogLevel level = LogLevel::kWarning);

  void add_handler(std::shared_ptr<Handler>);
  void handle(const Record& record);

  void set_level(LogLevel level) { level_ = level; }
  LogLevel get_level() const { return level_; }
  const std::string& get_name() const { return name_; }

 private:
  std::string name_;
  LogLevel level_;
  std::vector<std::shared_ptr<Handler>> handlers_;
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
class StreamHandler : public Handler {
 public:
  explicit StreamHandler(std::ostream&);

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
class FileHandler : public StreamHandler {
 public:
  explicit FileHandler(const Path& path);
  ~FileHandler();

 private:
  std::ofstream fstream_;
};

/** Set log level for all registered loggers. */
void set_log_level(LogLevel level);

/** Set log level for the named logger. */
void set_log_level(const char* name, LogLevel level);

/**
 * Register handler for all plugins.
 *
 * This will register a handler for all plugins that have been
 * registered with the logging subsystem (normally all plugins that
 * have been loaded by `Loader`). For example, to register a custom
 * handler from a plugin, you would do the following:
 *
 * @code
 * void init() {
 *   ...
 *   register_handler(std::make_shared<MyHandler>(...));
 *   ...
 * }
 * @endcode
 */
void register_handler(std::shared_ptr<Handler>);

/**
 * Log message for the named module.
 *
 * This will log an error, warning, informational, or debug message
 * for the given module. The module have to be be registered before
 * anything is being logged. The `Loader` uses the plugin name as the
 * module name, so normally you should provide the plugin name as the
 * first argument to this function.
 *
 * @param name Module name to use.
 *
 * @param fmt `printf`-style format string, with arguments following.
 */
/** @{ */
#ifdef __cplusplus
extern "C" {
#endif

void LOGGER_API log_error(const char* name, const char *fmt, ...);
void LOGGER_API log_warning(const char* name, const char *fmt, ...);
void LOGGER_API log_info(const char* name, const char *fmt, ...);
void LOGGER_API log_debug(const char* name, const char *fmt, ...);

#ifdef WITH_DEBUG
#define log_debug2(args) log_debug args
#define log_debug3(args) log_debug args
#else
#define log_debug2(args) do {;} while (0)
#define log_debug3(args) do {;} while (0)
#endif
/** @} */

#ifdef __cplusplus
}
#endif

}  // namespace logging

}  // namespace mysql_harness
#endif /* MYSQL_HARNESS_LOGGER_INCLUDED */
