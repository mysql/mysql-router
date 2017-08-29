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

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

#include <iostream>
#include <cstdlib>

#ifndef _WIN32
#  include <unistd.h>
#else
#  include <windows.h>
#endif

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::ConfigSection;
using mysql_harness::PluginFuncEnv;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;
using mysql_harness::logging::log_info;

#ifdef WIN32
#  define EXAMPLE_IMPORT __declspec(dllimport)
#else
#  define EXAMPLE_IMPORT
#endif

extern "C" {
  extern void EXAMPLE_IMPORT do_magic();
}


#if defined(_MSC_VER) && defined(harness_example_EXPORTS)
/* We are building this library */
#  define EXAMPLE_API __declspec(dllexport)
#else
#  define EXAMPLE_API
#endif

static const char* requires[] = {
  "magic (>>1.0)",
};

static void init(PluginFuncEnv*);
static void deinit(PluginFuncEnv*);
static void start(PluginFuncEnv*);

extern "C" {
  Plugin EXAMPLE_API harness_plugin_example = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "An example plugin",
    VERSION_NUMBER(1, 0, 0),
    sizeof(requires) / sizeof(*requires),
    requires,
    0,
    nullptr,  // conflicts
    init,     // init
    deinit,   // deinit
    start,    // start
    nullptr,  // stop
  };

}

static void init(PluginFuncEnv*) {
  do_magic();
}

static void deinit(PluginFuncEnv*) {
}

static void start(PluginFuncEnv*) {
  for (int x = 0 ; x < 10 ; ++x) {
    log_info("example", "<count: %d>", x);
#ifndef _WIN32
    sleep(1);
#else
    Sleep(1000);
#endif
  }
}

