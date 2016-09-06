/*
  Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "metadata_cache.h"
#include "plugin_config.h"

#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"
#include "logger.h"
#include "config_parser.h"

using metadata_cache::LookupResult;
using mysqlrouter::TCPAddress;
using std::string;

const mysql_harness::AppInfo *g_app_info;
static const string kSectionName = "metadata_cache";

static const char *kRoutingRequires[] = {
    "logger",
};

/**
 * Load the metadata cache configuration from the router config file.
 *
 * @param info the object encapuslating the router configuration.
 *
 * @return 0 if the configuration was read successfully.
 */
static int init(const mysql_harness::AppInfo *info) {
  g_app_info = info;
  // If a valid configuration object was found.
  if (info && info->config) {
    // if a valid metadata_cache section was found in the router
    // configuration.
    if (info->config->get(kSectionName).empty()) {
      throw std::invalid_argument("[metadata_cache] section is empty");
    }
  }
  return 0;
}

/**
 * Initialize the metadata cache for fetching the information from the
 * metadata servers.
 *
 * @param section An object encapsulating the metadata cache information.
 */
static void start(const mysql_harness::ConfigSection *section) {
 try {
    MetadataCachePluginConfig config(section);
    unsigned int ttl{config.ttl};
    string metadata_cluster{config.metadata_cluster};

    // Initialize the defaults.
    ttl = ttl == 0 ? metadata_cache::kDefaultMetadataTTL : ttl;
    metadata_cluster = metadata_cluster.empty()?
      metadata_cache::kDefaultMetadataCluster : metadata_cluster;

    log_info("Starting Metadata Cache");

    // Initialize the metadata cache.
    metadata_cache::cache_init(config.bootstrap_addresses, config.user,
                               config.password, ttl,
                               metadata_cluster);
  } catch (const std::runtime_error &exc) {
    // We continue and retry
    log_error(exc.what());
  } catch (const std::invalid_argument &exc) {
    log_error(exc.what());
    return;
  }
}

extern "C" {

mysql_harness::Plugin METADATA_API harness_plugin_metadata_cache = {
    mysql_harness::PLUGIN_ABI_VERSION,
    mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "Metadata Cache, managing information fetched from the Metadata Server",
    VERSION_NUMBER(0, 0, 1),
    sizeof(kRoutingRequires) / sizeof(*kRoutingRequires), kRoutingRequires, // Requires
    0, NULL,                                      // Conflicts
    init,
    NULL,
    start,                                       // start
    NULL                                         // stop
};

}
