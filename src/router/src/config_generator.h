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

#ifndef ROUTER_CONFIG_GENERATOR_INCLUDED
#define ROUTER_CONFIG_GENERATOR_INCLUDED

#include <functional>
#include <map>
#include <vector>
#include <string>
#include <ostream>
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"

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
DECLARE_TEST(ConfigGeneratorTest, ssl_stage1_cmdline_arg_parse);
DECLARE_TEST(ConfigGeneratorTest, ssl_stage2_bootstrap_connection);
DECLARE_TEST(ConfigGeneratorTest, ssl_stage3_create_config);
DECLARE_TEST(ConfigGeneratorTest, empty_config_file);
DECLARE_TEST(ConfigGeneratorTest, warn_on_no_ssl);
DECLARE_TEST(ConfigGeneratorTest, set_file_owner_no_user);
DECLARE_TEST(ConfigGeneratorTest, set_file_owner_user_empty);
#endif

namespace mysqlrouter {
class MySQLSession;
class SysUserOperationsBase;
class SysUserOperations;

class ConfigGenerator {
public:
  ConfigGenerator(
#ifndef _WIN32
          SysUserOperationsBase* sys_user_operations = SysUserOperations::instance()
#endif
          )
    : mysql_(nullptr), mysql_owned_(false)
  #ifndef _WIN32
    , sys_user_operations_(sys_user_operations)
  #endif
  {}
  void init(const std::string &server_url, const std::map<std::string, std::string>& bootstrap_options);  // throws std::runtime_error
  void init(mysqlrouter::MySQLSession *session);
  bool warn_on_no_ssl(const std::map<std::string, std::string> &options); // throws std::runtime_error
  ~ConfigGenerator();

  void bootstrap_system_deployment(const std::string &config_file_path,
      const std::map<std::string, std::string> &options,
      const std::map<std::string, std::string> &default_paths,
      const std::string &default_keyring_path,
      const std::string &keyring_master_key_file);
  void bootstrap_directory_deployment(const std::string &directory,
      const std::map<std::string, std::string> &options,
      const std::map<std::string, std::string> &default_paths,
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
    std::string override_datadir;
    std::string socketsdir;

    std::string keyring_file_path;
    std::string keyring_master_key;
    std::string keyring_master_key_file_path;

    bool multi_master;
    std::string bind_address;

    mysqlrouter::SSLOptions ssl_options;
  };

  void set_file_owner(const std::map<std::string, std::string> &options,
                      const std::string &owner); // throws std::runtime_error

private:
  friend class MySQLInnoDBClusterMetadata;

  Options fill_options(bool multi_master,
      const std::map<std::string, std::string> &user_options);

  void create_start_scripts(const std::string &directory,
                            bool interactive_master_key,
                            const std::map<std::string, std::string> &options);

  void bootstrap_deployment(std::ostream &config_file,
      const mysql_harness::Path &config_file_path, const std::string &name,
      const std::map<std::string, std::string> &options,
      const std::map<std::string, std::string> &default_paths,
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
                     const std::string &system_username,
                     const std::string &bootstrap_server_addresses,
                     const std::string &metadata_cluster,
                     const std::string &metadata_replicaset,
                     const std::string &username,
                     const Options &options,
                     bool print_configs = false);

  // returns auto-generated password for the account
  std::string create_account(const std::map<std::string, std::string> &user_options,
                             const std::string &username);

  void create_account(const std::string &username, const std::string &password,
                      bool password_hashed = false);

  std::pair<uint32_t, std::string> get_router_id_and_name_from_config(const std::string &config_file_path,
                                          const std::string &cluster_name,
                                          bool forcing_overwrite);

  void update_router_info(uint32_t router_id, const Options &options);

  std::string endpoint_option(const Options &options, const Options::Endpoint &ep);

  bool backup_config_file_if_different(const mysql_harness::Path &config_path,
                                       const std::string &new_file_path,
                                       const std::map<std::string, std::string> &options);

  static void set_ssl_options(MySQLSession* sess,
                           const std::map<std::string, std::string>& options);
private:
  // TODO refactoring: these 3 should be removed (replaced by DIM semantics)
  MySQLSession *mysql_;
  bool mysql_owned_;
  std::function<void(mysqlrouter::MySQLSession*)> mysql_deleter_;

#ifndef _WIN32
  SysUserOperationsBase* sys_user_operations_;
#endif

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
  FRIEND_TEST(::ConfigGeneratorTest, ssl_stage1_cmdline_arg_parse);
  FRIEND_TEST(::ConfigGeneratorTest, ssl_stage2_bootstrap_connection);
  FRIEND_TEST(::ConfigGeneratorTest, ssl_stage3_create_config);
  FRIEND_TEST(::ConfigGeneratorTest, empty_config_file);
  FRIEND_TEST(::ConfigGeneratorTest, warn_on_no_ssl);
  FRIEND_TEST(::ConfigGeneratorTest, set_file_owner_no_user);
  FRIEND_TEST(::ConfigGeneratorTest, set_file_owner_user_empty);
#endif
};
}
#endif //ROUTER_CONFIG_GENERATOR_INCLUDED
