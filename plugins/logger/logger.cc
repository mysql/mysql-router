#include "logger.h"

#include <mysql/harness/plugin.h>
#include <mysql/harness/path_utils.h>

#include <string>
#include <cstdio>
#include <cassert>
#include <cstdarg>
#include <cstring>

typedef enum Level {
  LVL_FATAL,
  LVL_ERROR,
  LVL_WARNING,
  LVL_INFO,
  LVL_DEBUG,
  LEVEL_COUNT
} Level;

const char *level_str[] = {
  "FATAL", "ERROR", "WARNING", "INFO", "DEBUG", 0
};

static FILE* g_log_fd = NULL;

static int
init(AppInfo* info)
{
  // We allow the log directory to be NULL or empty, meaning that all
  // will go to the standard output.
  if (info->logdir == NULL || strlen(info->logdir) == 0) {
    g_log_fd = stdout;
  }
  else {
    const std::string
      log_file(mkpath(info->logdir, info->program, "log"));
    g_log_fd = fopen(log_file.c_str(), "a");
    if (!g_log_fd) {
      fprintf(stderr, "logger: could not open log file '%s' - %s",
              log_file.c_str(), strerror(errno));
      return 1;
    }
  }

  return 0;
}


static void
log_message(Level level, const char* fmt, va_list ap)
{
  assert(level < LEVEL_COUNT);
  assert(g_log_fd);

  // Format the message
  char message[256];
  vsnprintf(message, sizeof(message), fmt, ap);

  // Format the time (19 characters)
  char time_buf[20];
  time_t now;
  time(&now);
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

  // Emit a message on log file (or stdout).
  fprintf(g_log_fd, "%-20s %-8s %s\n", time_buf, level_str[level], message);
}


// Log format is:
// <date> <level> <plugin> <message>

void log_error(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  log_message(LVL_ERROR, fmt, args);
  va_end(args);
}


void log_warning(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  log_message(LVL_WARNING, fmt, args);
  va_end(args);
}


void log_info(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  log_message(LVL_INFO, fmt, args);
  va_end(args);
}


void log_debug(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  log_message(LVL_DEBUG, fmt, args);
  va_end(args);
}


Plugin logger = {
  PLUGIN_ABI_VERSION,
  "Logging functions",
  VERSION_NUMBER(0,0,1),
  0, NULL,                                      // Requires
  0, NULL,                                      // Conflicts
  init,
  NULL,
  NULL                                          // start
};
