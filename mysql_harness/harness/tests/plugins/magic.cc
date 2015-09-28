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
#include "config_parser.h"

#include <cstdlib>
#include <iostream>

const AppInfo* g_info;
const ConfigSection* g_section;

static int init(const AppInfo* info) {
  g_info = info;
  return 0;
}

void do_magic() {
  auto&& section = g_info->config->get("magic", "");
  auto&& message = section.get("message");
  log_info("%s", message.c_str());
}

Plugin harness_plugin_magic = {
  PLUGIN_ABI_VERSION,
  ARCHITECTURE_DESCRIPTOR,
  "A magic plugin",
  VERSION_NUMBER(1,2,3),
  0,
  nullptr,
  0,
  nullptr,
  init,
  nullptr,                                      // deinit
  nullptr,                                      // start
};
