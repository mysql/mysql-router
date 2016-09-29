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

#ifndef MYSQL_HARNESS_LOGGER_INCLUDED
#define MYSQL_HARNESS_LOGGER_INCLUDED

#include <mysql/harness/plugin.h>

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

#ifdef __cplusplus
extern "C" {
#endif

void LOGGER_API log_error(const char *fmt, ...);
void LOGGER_API log_warning(const char *fmt, ...);
void LOGGER_API log_info(const char *fmt, ...);
void LOGGER_API log_debug(const char *fmt, ...);

#ifdef WITH_DEBUG
#define log_debug2(args) log_debug args
#define log_debug3(args) log_debug args
#else
#define log_debug2(args) do {;} while (0)
#define log_debug3(args) do {;} while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* MYSQL_HARNESS_LOGGER_INCLUDED */
