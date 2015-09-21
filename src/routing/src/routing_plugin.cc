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

#include "plugin_config.h"
#include "mysql_routing.h"
#include "utils.h"

#include "logger.h"
#include "config_parser.h"

#include <iostream>
#include <mutex>
#include <vector>

using std::string;

const AppInfo *g_app_info;

const char *kRoutingRequires[1] = {
    "logger",
};

static int init(const AppInfo *info) {
  g_app_info = info;
  return 0;
}

static void start(const ConfigSection *section) {
  auto name = section->name + ":" + section->key;

  try {
    RoutingPluginConfig config(section);
    config.section_name = name;
    MySQLRouting r(config.mode, config.bind_address.port,
                   config.bind_address.addr, name, config.max_connections, config.wait_timeout, config.connect_timeout);
    try {
      r.set_destinations_from_uri(URI(config.destinations));
    } catch (URIError) {
      r.set_destinations_from_csv(config.destinations);
    }
    r.start();
  } catch (const std::invalid_argument &exc) {
    log_error(exc.what());
    return;
  } catch (const std::exception &exc) {
    log_error("%s: %s", name.c_str(), exc.what());
  }
}

Plugin harness_plugin_routing = {
    PLUGIN_ABI_VERSION,
    "Routing MySQL connections between MySQL clients/connectors and servers",
    VERSION_NUMBER(0, 0, 1),
    sizeof(kRoutingRequires) / sizeof(*kRoutingRequires), kRoutingRequires, // Requires
    0, nullptr,                                  // Conflicts
    init,
    nullptr,
    start                                        // start
};
