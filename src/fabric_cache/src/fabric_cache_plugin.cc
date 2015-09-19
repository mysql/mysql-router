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

#include "fabric_cache.h"
#include "plugin_config.h"

#include <string>

#include "logger.h"
#include "config_parser.h"

using std::string;
using fabric_cache::LookupResult;

const AppInfo *g_app_info;

static const char *kRoutingRequires[] = {
    "logger",
};

static int init(const AppInfo *info) {
  g_app_info = info;
  return 0;
}

static void start(const ConfigSection *section) {
  std::string section_name = section->name + ":" + section->key;
  string name_tag = string();

  if (!section->key.empty()) {
    name_tag = "'" + section->key + "' ";
  }

  try {
    FabricCachePluginConfig config(section);
    int port{config.address.port};

    port = port == 0 ? fabric_cache::kDefaultFabricPort : port;

    log_info("Starting Fabric Cache %susing MySQL Fabric running on %s",
             name_tag.c_str(), config.address.str().c_str());
    fabric_cache::cache_init(section->key, config.address.addr, port, config.user, config.password);

  } catch (const fabric_cache::base_error &exc) {
    // We continue and retry
    log_error(exc.what());
  } catch (const std::invalid_argument &exc) {
    log_error(exc.what());
    return;
  }
}

Plugin harness_plugin_fabric_cache = {
    PLUGIN_ABI_VERSION,
    "Fabric Cache, managing information fetched from MySQL Fabric",
    VERSION_NUMBER(0, 0, 1),
    sizeof(kRoutingRequires) / sizeof(*kRoutingRequires), kRoutingRequires, // Requires
    0, NULL,                                      // Conflicts
    init,
    NULL,
    start                                        // start
};
