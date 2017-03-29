/*
  Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include "mysqlrouter/mysql_session.h"  // kSslModePreferred

#include <string>
#include <thread>
#ifndef _WIN32
#  include <termios.h>
#  include <unistd.h>
#endif

#include "keyring/keyring_manager.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"
#include "logger.h"
#include "config_parser.h"

using metadata_cache::LookupResult;
using mysqlrouter::TCPAddress;
using std::string;

const mysql_harness::AppInfo *g_app_info;
static const string kSectionName = "metadata_cache";
static const char *kKeyringAttributePassword = "password";

static const char *kRoutingRequires[] = {
    "logger",
};

// FIXME
#define log_debug(...)    mysql_harness::logging::log_debug("MC", __VA_ARGS__)
#define log_info(...)     mysql_harness::logging::log_info("MC", __VA_ARGS__)
#define log_warning(...)  mysql_harness::logging::log_warning("MC", __VA_ARGS__)
#define log_error(...)    mysql_harness::logging::log_error("MC", __VA_ARGS__)

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

static std::string get_option(const mysql_harness::ConfigSection *section,
                              const std::string &key,
                              const std::string &def_value) {
  if (section->has(key))
    return section->get(key);
  return def_value;
}

static mysqlrouter::SSLOptions make_ssl_options(
    const mysql_harness::ConfigSection *section) {
  mysqlrouter::SSLOptions options;

  options.mode = get_option(section, "ssl_mode", mysqlrouter::MySQLSession::kSslModePreferred);
  options.cipher = get_option(section, "ssl_cipher", "");
  options.tls_version = get_option(section, "tls_version", "");
  options.ca = get_option(section, "ssl_ca", "");
  options.capath = get_option(section, "ssl_capath", "");
  options.crl = get_option(section, "ssl_crl", "");
  options.crlpath = get_option(section, "ssl_crlpath", "");

  return options;
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

    std::string password;
    try {
      password = mysql_harness::get_keyring() ?
        mysql_harness::get_keyring()->fetch(config.user,
                                            kKeyringAttributePassword) : "";
    }
    catch (const std::out_of_range&) {
      std::string msg = "Could not find the password for user '" + config.user + "' in the keyring. "
              "metadata_cache not initialized properly.";
      throw std::runtime_error(msg);
    }

    log_info("Starting Metadata Cache");

    // Initialize the metadata cache.
    metadata_cache::cache_init(config.bootstrap_addresses, config.user,
                               password, ttl,
                               make_ssl_options(section),
                               metadata_cluster);
  } catch (const std::runtime_error &exc) { // metadata_cache::metadata_error inherits from runtime_error
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
