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

#include <map>
#include <vector>
#include <string>
#include <ostream>

namespace mysql_harness {
  class Path;
}

// GCC 4.8.4 requires all classes to be forward-declared before used with "friend class <friendee>",
// if they're in a different namespace than the friender
#ifdef FRIEND_TEST
#include "mysqlrouter/utils.h"  // DECLARE_TEST
DECLARE_TEST(ConfigGeneratorTest, fetch_bootstrap_servers_one);
DECLARE_TEST(ConfigGeneratorTest, fetch_bootstrap_servers_three);
DECLARE_TEST(ConfigGeneratorTest, fetch_bootstrap_servers_multiple_replicasets);
DECLARE_TEST(ConfigGeneratorTest, fetch_bootstrap_servers_invalid);
DECLARE_TEST(ConfigGeneratorTest, create_config_single_master);
DECLARE_TEST(ConfigGeneratorTest, create_config_multi_master);
DECLARE_TEST(ConfigGeneratorTest, create_acount);
DECLARE_TEST(ConfigGeneratorTest, fill_options);
DECLARE_TEST(ConfigGeneratorTest, bootstrap_invalid_name);
DECLARE_TEST(ConfigGeneratorTest, empty_config_file);
#endif

namespace mysqlrouter {
class MySQLSession;

class ConfigGenerator {
public:
  ConfigGenerator()
    : mysql_(nullptr), mysql_owned_(false) {}
  void init(const std::string &server_url);
  void init(mysqlrouter::MySQLSession *session);
  ~ConfigGenerator();

  void bootstrap_system_deployment(const std::string &config_file_path,
      const std::map<std::string, std::string> &options,
      const std::string &default_keyring_path,
      const std::string &keyring_master_key_file);
  void bootstrap_directory_deployment(const std::string &directory,
      const std::map<std::string, std::string> &options,
      const std::string &default_keyring_file_name,
      const std::string &keyring_master_key_file);

  struct Options {
    struct Endpoint {
      int port;
      std::string socket;
      Endpoint() : port(0) {}
      Endpoint(const std::string &path) : port(0), socket(path) {}
      Endpoint(int port_) : port(port_) {}

      operator bool() const { return port > 0 || !socket.empty(); }
    };
    Options() : multi_master(false) {}

    Endpoint rw_endpoint;
    Endpoint ro_endpoint;
    Endpoint rw_x_endpoint;
    Endpoint ro_x_endpoint;

    std::string override_logdir;
    std::string override_rundir;
    std::string socketsdir;

    std::string keyring_file_path;
    std::string keyring_master_key;
    std::string keyring_master_key_file_path;

    bool multi_master;
    std::string bind_address;
  };
private:
  friend class MySQLInnoDBClusterMetadata;

  Options fill_options(bool multi_master,
      const std::map<std::string, std::string> &user_options);

  void create_start_scripts(const std::string &directory,
                            bool interactive_master_key);

  void bootstrap_deployment(std::ostream &config_file,
      const mysql_harness::Path &config_file_path, const std::string &name,
      const std::map<std::string, std::string> &options,
      const std::string &keyring_file,
      const std::string &keyring_master_key_file,
      bool directory_deployment);

  void init_keyring_file(const std::string &keyring_file,
                         const std::string &keyring_master_key_file);

  void fetch_bootstrap_servers(std::string &bootstrap_servers,
                               std::string &metadata_cluster,
                               std::string &metadata_replicaset,
                               bool &multi_master);
  void create_config(std::ostream &config_file,
                     uint32_t router_id,
                     const std::string &router_name,
                     const std::string &bootstrap_server_addresses,
                     const std::string &metadata_cluster,
                     const std::string &metadata_replicaset,
                     const std::string &username,
                     const Options &options,
                     bool print_configs = false);

  void create_account(const std::string &username, const std::string &password);

  uint32_t get_router_id_from_config_file(const std::string &config_file_path,
                                          const std::string &cluster_name,
                                          bool forcing_overwrite);

  void update_router_info(uint32_t router_id, const Options &options);

  std::string endpoint_option(const Options &options, const Options::Endpoint &ep);

  bool backup_config_file_if_different(const mysql_harness::Path &config_path,
                                       const std::string &new_file_path);
private:
  MySQLSession *mysql_;
  bool mysql_owned_;

#ifdef FRIEND_TEST
  FRIEND_TEST(::ConfigGeneratorTest, fetch_bootstrap_servers_one);
  FRIEND_TEST(::ConfigGeneratorTest, fetch_bootstrap_servers_three);
  FRIEND_TEST(::ConfigGeneratorTest, fetch_bootstrap_servers_multiple_replicasets);
  FRIEND_TEST(::ConfigGeneratorTest, fetch_bootstrap_servers_invalid);
  FRIEND_TEST(::ConfigGeneratorTest, create_config_single_master);
  FRIEND_TEST(::ConfigGeneratorTest, create_config_multi_master);
  FRIEND_TEST(::ConfigGeneratorTest, create_acount);
  FRIEND_TEST(::ConfigGeneratorTest, fill_options);
  FRIEND_TEST(::ConfigGeneratorTest, bootstrap_invalid_name);
  FRIEND_TEST(::ConfigGeneratorTest, empty_config_file);
#endif
};
}
#endif //ROUTER_CONFIG_GENERATOR_INCLUDED
