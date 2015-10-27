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


#include "helper_logger.h"

#include <atomic>
#include <cassert>
#include <cstdarg>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

using std::setw;

enum Level {
  LVL_FATAL,
  LVL_ERROR,
  LVL_WARNING,
  LVL_INFO,
  LVL_DEBUG,
  LEVEL_COUNT
};

static const char *const level_str[] = {
    "FATAL", "ERROR", "WARNING", "INFO", "DEBUG", 0
};

static void log_message(Level level, const char *fmt, va_list ap) {
  assert(level < LEVEL_COUNT);

  // Format the message
  char message[512];
  vsnprintf(message, sizeof(message), fmt, ap);

  // Format the time (19 characters)
  char time_buf[20];
  time_t now;
  time(&now);
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

  // Get the thread ID
  std::stringstream ss;
  ss << std::hex << std::this_thread::get_id();

  std::string thread_id = ss.str();
  if (thread_id.size() > 2 && thread_id.at(1) == 'x') {
    thread_id.erase(0, 2);
  }

  std::ostringstream log_entry;
  log_entry << setw(19) << time_buf << " " << setw(7) << level_str[level]
            << " [" << setw(7) << thread_id << "] " << message << "\n";

  std::cout << log_entry.str();
}


// Log format is:
// <date> <level> <plugin> <message>

void log_error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(LVL_ERROR, fmt, args);
  va_end(args);
}


void log_warning(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(LVL_WARNING, fmt, args);
  va_end(args);
}


void log_info(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(LVL_INFO, fmt, args);
  va_end(args);
}


void log_debug(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(LVL_DEBUG, fmt, args);
  va_end(args);
}
