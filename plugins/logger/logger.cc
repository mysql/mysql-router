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

#include "logger.h"

#include "plugin.h"
#include "filesystem.h"
#include "config_parser.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <thread>

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

static const std::map<std::string, Level> map_level_str = {
    {level_str[0], LVL_FATAL},
    {level_str[1], LVL_ERROR},
    {level_str[2], LVL_WARNING},
    {level_str[3], LVL_INFO},
    {level_str[4], LVL_DEBUG},
};

static std::atomic<FILE*> g_log_file;
static std::atomic<int> g_log_level;

static int init(const AppInfo* info) {
  // Default log level is INFO
  g_log_level = LVL_INFO;

  if (info && info->config) {
    auto sections = info->config->get("logger");
    if (sections.size() != 1) {
      throw std::invalid_argument("Section [logger] can only appear once");
    }
    auto section = sections.front();

    if (section->has("level")) {
      auto level_value = section->get("level");
      std::transform(level_value.begin(), level_value.end(), level_value.begin(), ::toupper);
      auto level = map_level_str.find(level_value);
      // Invalid values are reported as error
      if (level == map_level_str.end()) {
        throw std::invalid_argument(
            "Log level '" + level_value + "' is not valid; valid are " + level_str[0] + ", " +
                level_str[1] + ", " + level_str[2] + ", " + level_str[3] + ", or " + level_str[4]);
      }
      g_log_level = level->second;
    }
  }
  // We allow the log directory to be NULL or empty, meaning that all
  // will go to the standard output.
  if (info->logging_folder == NULL || strlen(info->logging_folder) == 0) {
    g_log_file.store(stdout, std::memory_order_release);
  } else {
    const auto log_file = Path::make_path(info->logging_folder, info->program, "log");
    FILE *fp = fopen(log_file.c_str(), "a");
    if (!fp) {
      fprintf(stderr, "logger: could not open log file '%s' - %s",
              log_file.c_str(), strerror(errno));
      fflush(stderr);
      return 1;
    }
    g_log_file.store(fp, std::memory_order_release);
  }

  return 0;
}

static int deinit(const AppInfo*) {
  assert(g_log_file);
  return fclose(g_log_file.exchange(nullptr, std::memory_order_acq_rel));
}

static void log_message(Level level, const char* fmt, va_list ap) {
  assert(level < LEVEL_COUNT);

  // Format the message
  char message[256];
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
  if (thread_id.at(1) == 'x') {
    thread_id.erase(0, 2);
  }

  // Emit a message on log file (or stdout).
  FILE *outfp = g_log_file.load(std::memory_order_consume);
  fprintf(outfp ? outfp : stdout, "%-19s %-7s [%s] %s\n",
          time_buf, level_str[level], thread_id.c_str(), message);
  fflush(outfp);
}


// Log format is:
// <date> <level> <plugin> <message>

void log_error(const char *fmt, ...) {
  if (g_log_level < LVL_ERROR)
    return;
  va_list args;
  va_start(args, fmt);
  log_message(LVL_ERROR, fmt, args);
  va_end(args);
}


void log_warning(const char *fmt, ...) {
  if (g_log_level < LVL_WARNING)
    return;
  va_list args;
  va_start(args, fmt);
  log_message(LVL_WARNING, fmt, args);
  va_end(args);
}


void log_info(const char *fmt, ...) {
  if (g_log_level < LVL_INFO)
    return;
  va_list args;
  va_start(args, fmt);
  log_message(LVL_INFO, fmt, args);
  va_end(args);
}


void log_debug(const char *fmt, ...) {
  if (g_log_level < LVL_DEBUG)
    return;
  va_list args;
  va_start(args, fmt);
  log_message(LVL_DEBUG, fmt, args);
  va_end(args);
}


Plugin logger = {
  PLUGIN_ABI_VERSION,
  ARCHITECTURE_DESCRIPTOR,
  "Logging functions",
  VERSION_NUMBER(0,0,1),
  0, NULL,                                      // Requires
  0, NULL,                                      // Conflicts
  init,
  deinit,
  NULL                                          // start
};
