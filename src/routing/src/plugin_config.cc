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
#include "mysqlrouter/routing.h"

#include <algorithm>
#include <vector>

#include "mysqlrouter/utils.h"

using std::vector;

string RoutingPluginConfig::get_default(const string &option) {

  const std::map<string, string> defaults{
      {"connect_timeout", to_string(routing::kDefaultDestinationConnectionTimeout)},
      {"wait_timeout",    to_string(routing::kDefaultWaitTimeout)},
      {"max_connections", to_string(routing::kDefaultMaxConnections)},
  };

  auto it = defaults.find(option);
  if (it == defaults.end()) {
    return string();
  }
  return it->second;
}

bool RoutingPluginConfig::is_required(const string &option) {
  const vector<string> required{
      "bind_address",
      "mode",
      "destinations",
  };

  return std::find(required.begin(), required.end(), option) != required.end();
}

routing::AccessMode RoutingPluginConfig::get_option_mode(const ConfigSection *section, const string &option) {
  string value;
  string valid;

  for (auto &it: routing::kAccessModeNames) {
    valid += it.first + ", ";
  }
  valid.erase(valid.size() - 2, 2);  // remove the extra ", "

  try {
    value = get_option_string(section, option);
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
  } catch (const invalid_argument) {
    throw invalid_argument(get_log_prefix(option) + " needs to be specified; valid are " + valid);
  }

  auto lookup = routing::kAccessModeNames.find(value);
  if (lookup == routing::kAccessModeNames.end()) {
    throw invalid_argument(get_log_prefix(option) + "is invalid; valid are " + valid + " (was '" + value + "')");
  }

  return lookup->second;
}
