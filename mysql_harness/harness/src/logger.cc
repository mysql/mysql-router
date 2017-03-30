/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

/** @file
 * Module for implementing the Logger functionality.
 */

#include "logger.h"

#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/plugin.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#ifndef _WINDOWS
#  include <sys/types.h>
#  include <unistd.h>
#endif

using mysql_harness::Path;

#if defined(_MSC_VER) && defined(logger_EXPORTS)
/* We are building this library */
#  define LOGGER_API __declspec(dllexport)
#else
#  define LOGGER_API
#endif

using std::ofstream;
using std::ostringstream;

static const char *const level_str[] = {
  "FATAL", "ERROR", "WARNING", "INFO", "DEBUG"
};

namespace mysql_harness {

namespace logging {

////////////////////////////////////////////////////////////////
// class Handler

Handler::Handler(LogLevel level) : level_(level) {}

// Log format is:
// <date> <time> <plugin> <level> [<thread>] <message>

std::string Handler::format(const Record& record) const {
  // Format the time (19 characters)
  char time_buf[20];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S",
           localtime(&record.created));

  // Get the thread ID in a printable format
  std::stringstream ss;
  ss << std::hex << std::this_thread::get_id();

  // We ignore the return value from snprintf, which means that the
  // output is truncated if the total length exceeds the buffer size.
  char buffer[512];
  snprintf(buffer, sizeof(buffer), "%-19s %s %s [%s] %s",
           time_buf, record.domain.c_str(),
           level_str[static_cast<int>(record.level)],
           ss.str().c_str(), record.message.c_str());

  // Note: This copy the buffer into an std::string
  return buffer;
}

void Handler::handle(const Record& record) {
  do_log(record);
}

////////////////////////////////////////////////////////////////
// class StreamHandler

StreamHandler::StreamHandler(std::ostream& out, LogLevel level)
    : Handler(level), stream_(out) {}

void StreamHandler::do_log(const Record& record) {
  std::lock_guard<std::mutex> lock(stream_mutex_);
  stream_ << format(record) << std::endl;
}

////////////////////////////////////////////////////////////////
// class FileHandler

FileHandler::FileHandler(const Path& path, LogLevel level)
    : StreamHandler(fstream_, level),
      fstream_(path.str(), ofstream::app) {
  if (fstream_.fail()) {
    ostringstream buffer;
    char buf[1024];
    strerror_r(errno, buf, sizeof(buf));
    buffer << "Failed to open " << path << ": " << buf;
    throw std::runtime_error(buffer.str());
  }
}

FileHandler::~FileHandler() {}

////////////////////////////////////////////////////////////////
// class Logger

Logger::Logger(const std::string& name, LogLevel level)
    : name_(name), level_(level) {}

void Logger::add_handler(std::shared_ptr<Handler> handler) {
  handlers_.push_back(handler);
}

void Logger::remove_handler(std::shared_ptr<Handler> handler) {
  auto it = std::find(handlers_.begin(), handlers_.end(), handler);
  if (it != handlers_.end())
    handlers_.erase(it);
}

void Logger::handle(const Record& record) {
  if (record.level <= level_) {
    for (auto&& handler : handlers_)
      if (record.level <= handler->get_level())
        handler->handle(record);
  }
}

}

}
