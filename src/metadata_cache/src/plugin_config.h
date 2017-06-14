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

#ifndef METADATA_CACHE_PLUGIN_CONFIG_INCLUDED
#define METADATA_CACHE_PLUGIN_CONFIG_INCLUDED

#include "mysqlrouter/metadata_cache.h"

#include <map>
#include <string>
#include <vector>

#include "config_parser.h"
#include "mysql/harness/plugin.h"
#include <mysqlrouter/datatypes.h>
#include <mysqlrouter/plugin_config.h>


extern "C"
{
  extern mysql_harness::Plugin METADATA_API harness_plugin_metadata_cache ;
}

class MetadataCachePluginConfig final : public mysqlrouter::BasePluginConfig {
public:
  /** @brief Constructor
   *
   * @param section from configuration file provided as ConfigSection
   */
  MetadataCachePluginConfig(const mysql_harness::ConfigSection *section)
      : BasePluginConfig(section),
        bootstrap_addresses(get_bootstrap_servers(
                              section, "bootstrap_server_addresses",
                              metadata_cache::kDefaultMetadataPort)),
        user(get_option_string(section, "user")),
        ttl(get_uint_option<unsigned int>(section, "ttl")),
        metadata_cluster(get_option_string(section, "metadata_cluster"))
        { }

  /**
   * @param option name of the option
   */
  std::string get_default(const std::string &option);
  bool is_required(const std::string &option);

  /** @brief MySQL Metadata host to connect with */
  const std::vector<mysqlrouter::TCPAddress> bootstrap_addresses;
  /** @brief User used for authenticating with MySQL Metadata */
  const std::string user;
  /** @brief TTL used for storing data in the cache */
  const unsigned int ttl;
  /** @brief Cluster in the metadata */
  const std::string metadata_cluster;

private:
  /** @brief Gets a list of metadata servers.
   *
   *
   * Throws std::invalid_argument on errors.
   *
   * @param section Instance of ConfigSection
   * @param option Option name in section
   * @param default_port Use this port when none was provided
   * @return std::vector<mysqlrouter::TCPAddress>
   */
  std::vector<mysqlrouter::TCPAddress> get_bootstrap_servers(
    const mysql_harness::ConfigSection *section, const std::string &option,
    uint16_t default_port);
};

#endif // METADATA_CACHE_PLUGIN_CONFIG_INCLUDED
