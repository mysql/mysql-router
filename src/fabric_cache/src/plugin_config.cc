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
#include "mysqlrouter/utils.h"
#include "mysqlrouter/fabric_cache.h"

#include <algorithm>
#include <exception>
#include <map>
#include <vector>

#include "logger.h"

using mysqlrouter::string_format;
using mysqlrouter::to_string;
using std::invalid_argument;

string FabricCachePluginConfig::get_default(const string &option) {

  const std::map<string, string> defaults{
      {"address",  fabric_cache::kDefaultFabricAddress},
  };

  auto it = defaults.find(option);
  if (it == defaults.end()) {
    return string();
  }
  return it->second;
}

bool FabricCachePluginConfig::is_required(const string &option) {
  const vector<string> required{
      "user",
  };

  return std::find(required.begin(), required.end(), option) != required.end();
}

mysqlrouter::TCPAddress FabricCachePluginConfig::get_option_tcp_address(const ConfigSection *section,
                                                                        const string &option,
                                                                        uint16_t default_port) {
  auto value = get_option_string(section, option);

  try {
    auto bind_info = mysqlrouter::split_addr_port(value);

    if (!bind_info.second) {
      bind_info.second = default_port;
    }

    return mysqlrouter::TCPAddress(bind_info.first, bind_info.second);

  } catch (const std::runtime_error &exc) {
    throw invalid_argument(get_log_prefix(option) + " is incorrect (" + exc.what() + ")");
  }

}
