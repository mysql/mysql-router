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

#ifndef FABRIC_CACHE_PLUGIN_CONFIG_INCLUDED
#define FABRIC_CACHE_PLUGIN_CONFIG_INCLUDED

#include "mysqlrouter/fabric_cache.h"

#include <map>
#include <string>
#include <vector>

#include "config_parser.h"
#include "plugin.h"
#include <mysqlrouter/datatypes.h>
#include <mysqlrouter/plugin_config.h>

using std::map;
using std::string;
using std::vector;

class FabricCachePluginConfig final : public mysqlrouter::BasePluginConfig {
public:
  /** @brief Constructor
   *
   * @param section from configuration file provided as ConfigSection
   */
  FabricCachePluginConfig(const ConfigSection *section)
      : BasePluginConfig(section),
        address(get_option_tcp_address(section, "address", fabric_cache::kDefaultFabricPort)),
        user(get_option_string(section, "user")) { }

  string get_default(const string &option);
  bool is_required(const string &option);

  /** @brief MySQL Fabric host to connect with */
  const mysqlrouter::TCPAddress address;
  /** @brief User used for authenticating with MySQL Fabric */
  const string user;

private:
  /** @brief Gets a TCP address using the given option
   *
   * Gets a TCP address using the given option. The option value is
   * split in 2 giving the IP (or address) and the TCP Port. When
   * no TCP port was found in the address, the default_port value
   * will be used.
   *
   * Throws std::invalid_argument on errors.
   *
   * @param section Instance of ConfigSection
   * @param option Option name in section
   * @param default_port Use this port when none was provided
   * @return mysqlrouter::TCPAddress
   */
  mysqlrouter::TCPAddress get_option_tcp_address(const ConfigSection *section,
                                                 const string &option,
                                                 uint16_t default_port);
};

#endif // FABRIC_CACHE_PLUGIN_CONFIG_INCLUDED
