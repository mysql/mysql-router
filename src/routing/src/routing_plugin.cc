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

#include "plugin_config.h"
#include "mysql_routing.h"
#include "utils.h"

#include "logger.h"
#include "config_parser.h"

#include <atomic>
#include <iostream>
#include <mutex>
#include <vector>

using std::string;
using mysqlrouter::URIError;

const AppInfo *g_app_info;
static const string kSectionName = "routing";

const char *kRoutingRequires[1] = {
    "logger",
};

static int init(const AppInfo *info) {
  if (info->config != nullptr) {
    bool have_fabric_cache = false;
    bool need_fabric_cache = false;
    std::vector<TCPAddress> bind_addresses;
    std::vector<uint16_t> ports;
    for (auto &section: info->config->sections()) {
      if (section->name == kSectionName) {
        string err_prefix = mysqlrouter::string_format("in [%s%s%s]: ", section->name.c_str(),
                                                       section->key.empty() ? "" : ":",
                                                       section->key.c_str());
        // Check the configuration
        RoutingPluginConfig config(section); // raises on errors

        auto config_addr = config.bind_address;

        // either bind_port or bind_address is required
        if (config.bind_port < 0 && !section->has("bind_address")) {
          throw std::invalid_argument(err_prefix + "either bind_port or bind_address is required");
        }

        // no bind_port and bind_address has no valid port
        if (config.bind_port < 0 &&
            !(section->has("bind_address") && config.bind_address.port > 0)) {
          throw std::invalid_argument(err_prefix + "no bind_port, and TCP port in bind_address is not valid");
        }

        if (!config_addr.is_valid()) {
          throw std::invalid_argument(err_prefix + "invalid IP or name in bind_address '" + config_addr.str() + "'");
        }

        // Check uniqueness of bind_address and port, using IP address
        auto found_addr = std::find(bind_addresses.begin(), bind_addresses.end(), config.bind_address);
        if (found_addr != bind_addresses.end()) {
          throw std::invalid_argument(err_prefix + "duplicate IP or name found in bind_address '" +
                                        config.bind_address.str() + "'");
        }

        // Check ADDR_ANY binding on same port
        if (config_addr.addr == "0.0.0.0" || config_addr.addr == "::") {
          auto found_addr = std::find_if(bind_addresses.begin(), bind_addresses.end(), [&config](TCPAddress &addr) {
            return config.bind_address.port == addr.port;
          });
          if (found_addr != bind_addresses.end()) {
            throw std::invalid_argument(
                err_prefix + "duplicate IP or name found in bind_address '" + config.bind_address.str() + "'");
          }
        }
        bind_addresses.push_back(config.bind_address);

        // We check if we need special plugins based on URI
        try {
          auto uri = URI(config.destinations);
          if (uri.scheme == "fabric+cache") {
            need_fabric_cache = true;
          }
        } catch (URIError) {
          // No URI, no extra plugin needed
        }
      } else if (section->name == "fabric_cache") {
        // We have fabric_cache
        have_fabric_cache = true;
      }
    }

    // Make sure we have at least one configuration for Fabric Cache when needed
    if (need_fabric_cache && !have_fabric_cache) {
      throw std::invalid_argument("Routing needs Fabric Cache, but no none was found in configuration.");
    }
  }
  g_app_info = info;
  return 0;
}

static void start(const ConfigSection *section) {
  string name;
  if (!section->key.empty()) {
    name = section->name + ":" + section->key;
  } else {
    name = section->name;
  }

  try {
    RoutingPluginConfig config(section);
    config.section_name = name;
    MySQLRouting r(config.mode, config.bind_address.port,
                   config.bind_address.addr, name, config.max_connections, config.connect_timeout,
                   config.max_connect_errors, config.client_connect_timeout);
    try {
      r.set_destinations_from_uri(URI(config.destinations));
    } catch (URIError) {
      r.set_destinations_from_csv(config.destinations);
    }
    r.start();
  } catch (const std::invalid_argument &exc) {
    log_error(exc.what());
    return;
  } catch (const std::runtime_error &exc) {
    log_error("%s: %s", name.c_str(), exc.what());
  }
}

Plugin harness_plugin_routing = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "Routing MySQL connections between MySQL clients/connectors and servers",
    VERSION_NUMBER(0, 0, 1),
    sizeof(kRoutingRequires) / sizeof(*kRoutingRequires), kRoutingRequires, // Requires
    0, nullptr,                                  // Conflicts
    init,
    nullptr,
    start                                        // start
};
