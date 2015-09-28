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

#include "plugin.h"
#include "logger.h"

#include <iostream>
#include <cstdlib>

#include <unistd.h>

static const char* requires[] = {
  "magic (>>1.0)",
  "logger",
};

static int init(const AppInfo*) {
  extern void do_magic();
  do_magic();
  return 0;
}

static int deinit(const AppInfo*) {
  return 0;
}

static void start(const ConfigSection*) {
  for (int x = 0 ; x < 10 ; ++x) {
    log_info("<count: %d>", x);
    sleep(1);
  }
}

Plugin example_plugin = {
  PLUGIN_ABI_VERSION,
  ARCHITECTURE_DESCRIPTOR,
  "An example plugin",
  VERSION_NUMBER(1,0,0),
  sizeof(requires)/sizeof(*requires),
  requires,
  0,
  nullptr,
  init,
  deinit,
  start,
};

