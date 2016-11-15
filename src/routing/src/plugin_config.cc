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
#include "mysqlrouter/routing.h"
#include "mysqlrouter/metadata_cache.h"

#include <algorithm>
#include <exception>
#include <vector>

#include "mysqlrouter/utils.h"

using std::invalid_argument;
using std::string;
using std::vector;
using mysqlrouter::URI;
using mysqlrouter::URIError;
using mysqlrouter::to_string;

//master:
/** @brief Constructor
 *
 * @param section from configuration file provided as ConfigSection
 */
RoutingPluginConfig::RoutingPluginConfig(const mysql_harness::ConfigSection *section)
    : BasePluginConfig(section),
      protocol(get_protocol(section, "protocol")),
      destinations(get_option_destinations(section, "destinations", protocol)),
      bind_port(get_option_tcp_port(section, "bind_port")),
      bind_address(get_option_tcp_address(section, "bind_address", false, bind_port)),
      named_socket(get_option_named_socket(section, "socket")),
      connect_timeout(get_uint_option<uint16_t>(section, "connect_timeout", 1)),
      mode(get_option_mode(section, "mode")),
      max_connections(get_uint_option<uint16_t>(section, "max_connections", 1)),
      max_connect_errors(get_uint_option<uint32_t>(section, "max_connect_errors", 1, UINT32_MAX)),
      client_connect_timeout(get_uint_option<uint32_t>(section, "client_connect_timeout", 2, 31536000)),
      net_buffer_length(get_uint_option<uint32_t>(section, "net_buffer_length", 1024, 1048576)) {

  // either bind_address or socket needs to be set, or both
  if (!bind_address.port && !named_socket.is_set()) {
    throw invalid_argument("either bind_address or socket option needs to be supplied, or both");
  }
}


string RoutingPluginConfig::get_default(const string &option) {

  const std::map<string, string> defaults{
      {"bind_address", to_string(routing::kDefaultBindAddress)},
      {"connect_timeout", to_string(routing::kDefaultDestinationConnectionTimeout)},
      {"max_connections", to_string(routing::kDefaultMaxConnections)},
      {"max_connect_errors", to_string(routing::kDefaultMaxConnectErrors)},
      {"client_connect_timeout", to_string(routing::kDefaultClientConnectTimeout)},
      {"net_buffer_length", to_string(routing::kDefaultNetBufferLength)},
  };

  auto it = defaults.find(option);
  if (it == defaults.end()) {
    return string();
  }
  return it->second;
}

bool RoutingPluginConfig::is_required(const string &option) {
  const vector<string> required{
      "mode",
      "destinations",
  };

  return std::find(required.begin(), required.end(), option) != required.end();
}

routing::AccessMode RoutingPluginConfig::get_option_mode(
    const mysql_harness::ConfigSection *section, const string &option) {
  string value;
  string valid;

  routing::get_access_mode_names(&valid);

  try {
    value = get_option_string(section, option);
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
  } catch (const invalid_argument) {
    throw invalid_argument(get_log_prefix(option) + " needs to be specified; valid are " + valid);
  }

  routing::AccessMode result = routing::get_access_mode(value);
  if (result == routing::AccessMode::kUndefined) {
    throw invalid_argument(get_log_prefix(option) + " is invalid; valid are " +
                           valid + " (was '" + value + "')");
  }
  return result;
}

Protocol::Type RoutingPluginConfig::get_protocol(const mysql_harness::ConfigSection *section,
                                                 const std::string &option) {
  std::string name;
  try {
    name = section->get(option);
  } catch (const mysql_harness::bad_option&) {
    return Protocol::get_default();
  }

  std::transform(name.begin(), name.end(), name.begin(), ::tolower);

  return Protocol::get_by_name(name);
}

string RoutingPluginConfig::get_option_destinations(const mysql_harness::ConfigSection *section,
                                                    const string &option,
                                                    const Protocol::Type &protocol_type) {
  bool required = is_required(option);
  string value;

  try {
    value = section->get(option);
  } catch (const mysql_harness::bad_option &exc) {
    if (required) {
      throw invalid_argument(get_log_prefix(option) + " is required");
    }
  }

  if (value.empty()) {
    if (required) {
      throw invalid_argument(get_log_prefix(option) + " is required and needs a value");
    }
    value = get_default(option);
  }

  try {
    auto uri = URI(value); // raises URIError when URI is invalid
    if (uri.scheme == "metadata-cache") {
    } else {
      throw invalid_argument(
          get_log_prefix(option) + " has an invalid URI scheme '" + uri.scheme + "' for URI " + value);
    }
    return value;
  } catch (URIError &) {
    char delimiter = ',';

    mysqlrouter::trim(value);
    if (value.back() == delimiter || value.front() == delimiter) {
      throw invalid_argument(get_log_prefix(option) +
                                 ": empty address found in destination list (was '" + value + "')");
    }

    std::stringstream ss(value);
    std::string part;
    std::pair<std::string, uint16_t> info;
    while (std::getline(ss, part, delimiter)) {
      mysqlrouter::trim(part);
      if (part.empty()) {
        throw invalid_argument(get_log_prefix(option) +
                                   ": empty address found in destination list (was '" + value + "')");
      }
      info = mysqlrouter::split_addr_port(part);
      if (info.second == 0) {
       info.second = Protocol::get_default_port(protocol_type);
      }
      mysqlrouter::TCPAddress addr(info.first, info.second);
      if (!addr.is_valid()) {
        throw invalid_argument(get_log_prefix(option) + " has an invalid destination address '" + addr.str() + "'");
      }
    }

  }

  return value;
}
