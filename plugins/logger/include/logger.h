#ifndef LOGGER_INCLUDED
#define LOGGER_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

void log_error(const char *fmt, ...);
void log_warning(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_debug(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_INCLUDED */
