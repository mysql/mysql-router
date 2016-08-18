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

#ifndef ROUTER_CONFIG_GENERATOR_INCLUDED
#define ROUTER_CONFIG_GENERATOR_INCLUDED

#include <vector>
#include <string>

class ConfigGenerator {
  public:
  ConfigGenerator(const std::string &origin)
  : origin_(origin) {}
  void fetch_bootstrap_servers(const std::string &server_url,
                               std::string &bootstrap_servers,
                               std::string &username,
                               std::string &password,
                               std::string &metadata_cluster,
                               std::string &metadata_replicaset);
  const std::string prompt_password(const std::string &prompt);
  void create_config(const std::string &bootstrap_server_addresses,
                     const std::string &metadat_cluster,
                     const std::string &metadata_replicaset,
                     const std::string &username,
                     const std::string &password);
  private:
  std::string origin_;
};

#endif //ROUTER_CONFIG_GENERATOR_INCLUDED
