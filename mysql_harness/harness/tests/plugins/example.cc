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

#include "mysql/harness/logger.h"
#include "mysql/harness/plugin.h"

#include <iostream>
#include <cstdlib>

#ifndef _WIN32
#  include <unistd.h>
#else
#  include <windows.h>
#endif

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::AppInfo;
using mysql_harness::ConfigSection;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;

#ifdef WIN32
#  define EXAMPLE_IMPORT __declspec(dllimport)
#else
#  define EXAMPLE_IMPORT
#endif

extern "C" {
  extern void EXAMPLE_IMPORT do_magic();
  extern void EXAMPLE_IMPORT log_info(const char *, ...);
}


#if defined(_MSC_VER) && defined(example_EXPORTS)
/* We are building this library */
#  define EXAMPLE_API __declspec(dllexport)
#else
#  define EXAMPLE_API
#endif

static const char* requires[] = {
  "magic (>>1.0)",
  "logger",
};

static int init(const AppInfo*);
static int deinit(const AppInfo*);
static void start(const ConfigSection*);

extern "C" {
  Plugin EXAMPLE_API example = {
  PLUGIN_ABI_VERSION,
  ARCHITECTURE_DESCRIPTOR,
  "An example plugin",
  VERSION_NUMBER(1, 0, 0),
  sizeof(requires) / sizeof(*requires),
  requires,
  0,
  nullptr,
  init,
  deinit,
  start,    // start
  nullptr,  // stop
};

}

static int init(const AppInfo*) {
  do_magic();
  return 0;
}

static int deinit(const AppInfo*) {
  return 0;
}

static void start(const ConfigSection*) {
  for (int x = 0 ; x < 10 ; ++x) {
    log_info("<count: %d>", x);
#ifndef _WIN32
    sleep(1);
#else
    Sleep(1000);
#endif
  }
}
