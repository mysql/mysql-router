#ifndef LOGGER_INCLUDED
#define LOGGER_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

void log_error(const char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));
void log_warning(const char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));
void log_info(const char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));
void log_debug(const char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_INCLUDED */
