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

#include "magic.h"

#include "mysql/harness/config_parser.h"
#include "mysql/harness/logger.h"
#include "mysql/harness/plugin.h"

#include <cstdlib>
#include <iostream>

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::AppInfo;
using mysql_harness::ConfigSection;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;

#if defined(_MSC_VER) && defined(magic_EXPORTS)
/* We are building this library */
#  define MAGIC_API __declspec(dllexport)
#else
#  define MAGIC_API
#endif

const AppInfo* g_info;
const ConfigSection* g_section;

static int init(const AppInfo* info) {
  g_info = info;
  return 0;
}

extern "C" void MAGIC_API do_magic() {
  auto&& section = g_info->config->get("magic", "");
  auto&& message = section.get("message");
  log_info("%s", message.c_str());
}

static void start(const ConfigSection* section) {
  if (section->get("suki") == "bad")
    throw bad_suki("The suki was bad, please throw away");
}

extern "C" {
  Plugin MAGIC_API magic = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "A magic plugin",
    VERSION_NUMBER(1, 2, 3),
    0,
    nullptr,
    0,
    nullptr,
    init,
    nullptr,  // deinit
    start,    // start
    nullptr,  // stop
  };
}
