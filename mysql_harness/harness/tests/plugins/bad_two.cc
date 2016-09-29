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

#include "mysql/harness/plugin.h"

using mysql_harness::AppInfo;
using mysql_harness::Plugin;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::ARCHITECTURE_DESCRIPTOR;

using mysql_harness::AppInfo;
using mysql_harness::Plugin;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::ARCHITECTURE_DESCRIPTOR;

static const char* requires[] = {
  // Magic plugin is version 1.2.3, so version does not match and this
  // should fail to load.
  "magic (>>1.2.3)",
};

static int init(const AppInfo*) {
  return 0;
}

static int deinit(const AppInfo*) {
  return 0;
}

#if defined(_MSC_VER) && defined(bad_two_EXPORTS)
/* We are building this library */
#  define EXAMPLE_API __declspec(dllexport)
#else
#  define EXAMPLE_API
#endif

extern "C" {
  Plugin EXAMPLE_API bad_two = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "A bad plugin",
    VERSION_NUMBER(1, 0, 0),
    sizeof(requires)/sizeof(*requires),
    requires,
    0,
    nullptr,
    init,
    deinit,
    nullptr,  // start
    nullptr,  // stop
  };
}
