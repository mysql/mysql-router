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

#include "keyring/keyring_manager.h"
#include "config_generator.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/uri.h"
#include "common.h"
#include "dim.h"
#include "filesystem.h"
#include "config_parser.h"
#include "rapidjson/rapidjson.h"
#include "random_generator.h"
#include "utils.h"
#include "router_app.h"
#include "sha1.h"

// #include "logger.h"
#ifdef _WIN32
#include <Windows.h>
#define strcasecmp _stricmp
#else
#include <sys/stat.h>
#endif


#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include "cluster_metadata.h"

static const int kDefaultRWPort = 6446;
static const int kDefaultROPort = 6447;
static const char *kRWSocketName = "mysql.sock";
static const char *kROSocketName = "mysqlro.sock";

static const int kDefaultRWXPort = 64460;
static const int kDefaultROXPort = 64470;
static const char *kRWXSocketName = "mysqlx.sock";
static const char *kROXSocketName = "mysqlxro.sock";

static const int kMaxTCPPortNumber = 65535;
static const int kAllocatedTCPPortCount = 4; // 2 for classic, 2 for X

static const std::string kSystemRouterName = "system";

static const int kMetadataServerPasswordLength = 16;
static const int kMaxRouterNameLength = 255; // must match metadata router.name column

static const char *kKeyringAttributePassword = "password";

static const int kDefaultTTL = 300; // default configured TTL in seconds
static constexpr uint32_t kMaxRouterId = 999999;  // max router id is 6 digits due to username size constraints
static constexpr unsigned kNumRandomChars = 12;
static constexpr unsigned kDefaultPasswordRetries = 20; // number of the retries when generating random password
                                                        // for the router user during the bootstrap
static constexpr unsigned kMaxPasswordRetries = 10000;

using mysql_harness::get_strerror;
using mysql_harness::Path;
using mysql_harness::UniquePtr;
using mysql_harness::DIM;
using namespace mysqlrouter;

namespace {
struct password_too_weak: public std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct plugin_not_loaded: public std::runtime_error {
  using std::runtime_error::runtime_error;
};
}


/**
 * Return a string representation of the input character string.
 *
 * @param input_str A character string.
 *
 * @return A string object encapsulation of the input character string. An empty
 *         string if input string is nullptr.
 */
static std::string get_string(const char *input_str) {
  if (input_str == nullptr) {
    return "";
  }
  return std::string(input_str);
}

static bool is_valid_name(const std::string& name) {
  if (!name.empty()) {
    for (char c : name) {
      if (c == '\n' || c=='\r')
        return false;
    }
  }
  return true;
}


class AutoCleaner {
public:
    void add_file_delete(const std::string &f) {
    _files[f] = File;
  }

  void add_directory_delete(const std::string &d, bool recursive = false) {
    _files[d] = recursive ? DirectoryRecursive : Directory;
  }

  void add_file_revert(const std::string &file) {
    if (mysql_harness::Path(file).is_regular()) {
      copy_file(file, file+".bck");
      _files[file] = FileBackup;
    } else {
      if (mysql_harness::Path(file+".bck").exists())
        delete_file(file+".bck");
      _files[file] = File;
    }
  }

  void remove(const std::string &p) {
    _files.erase(p);
  }

  void clear() {
    for (auto f = _files.rbegin(); f != _files.rend(); ++f) {
      if (f->second == FileBackup)
        delete_file(f->first+".bck");
    }
    _files.clear();
  }

  ~AutoCleaner() {
    // remove in reverse order, so that files are deleted before their
    // contained directories
    for (auto f = _files.rbegin(); f != _files.rend(); ++f) {
      switch (f->second) {
        case File:
          delete_file(f->first);
          break;

        case Directory:
          rmdir(f->first);
          break;

        case DirectoryRecursive:
          delete_recursive(f->first);
          break;

        case FileBackup:
          copy_file(f->first+".bck", f->first);
          delete_file(f->first+".bck");
          break;
      }
    }
  }
private:
  enum Type {
    Directory,
    DirectoryRecursive,
    File,
    FileBackup
  };
  std::map<std::string, Type> _files;
};


void ConfigGenerator::init(MySQLSession *session) {
  mysql_ = session; // TODO migrate to DIM and get rid of mysql_ and mysql_owned_

  check_innodb_metadata_cluster_session(session, false);
}

inline std::string get_opt(const std::map<std::string, std::string> &map,
                       const std::string &key, const std::string &default_value) {
  auto iter = map.find(key);
  if (iter == map.end())
    return default_value;
  return iter->second;
}

/*static*/
void ConfigGenerator::set_ssl_options(MySQLSession* sess,
                                   const std::map<std::string, std::string>& options) {

  std::string ssl_mode = get_opt(options, "ssl_mode", MySQLSession::kSslModePreferred);
  std::string ssl_cipher = get_opt(options, "ssl_cipher", "");
  std::string tls_version = get_opt(options, "tls_version", "");
  std::string ssl_ca = get_opt(options, "ssl_ca", "");
  std::string ssl_capath = get_opt(options, "ssl_capath", "");
  std::string ssl_crl = get_opt(options, "ssl_crl", "");
  std::string ssl_crlpath = get_opt(options, "ssl_crlpath", "");

// 2017.01.26: Disabling this code, since it's not part of GA v2.1.2.  It should be re-enabled later
#if 0
  std::string ssl_cert = get_opt(options, "ssl_cert", "");
  std::string ssl_key = get_opt(options, "ssl_key", "");
#endif

  // parse ssl_mode option (already validated in cmdline option handling)
  mysql_ssl_mode ssl_enum = MySQLSession::parse_ssl_mode(ssl_mode);

  // set ssl_mode
  sess->set_ssl_options(ssl_enum, tls_version, ssl_cipher,
                        ssl_ca, ssl_capath, ssl_crl, ssl_crlpath);

// 2017.01.26: Disabling this code, since it's not part of GA v2.1.2.  It should be re-enabled later
#if 0
  if (!ssl_cert.empty() || !ssl_key.empty()) {
    sess->set_ssl_cert(ssl_cert, ssl_key);
  }
#endif
}

bool ConfigGenerator::warn_on_no_ssl(const std::map<std::string, std::string> &options) {

  // warninng applicable only if --ssl-mode=PREFERRED (or not specified, which defaults to PREFERRED)
  std::string ssl_mode = get_opt(options, "ssl_mode", MySQLSession::kSslModePreferred);
  std::transform(ssl_mode.begin(), ssl_mode.end(), ssl_mode.begin(), toupper);

  if (ssl_mode != MySQLSession::kSslModePreferred)
    return true;

  // warn if the connection is unencrypted
  try {
    // example response
    //
    // > show status like "ssl_cipher"'
    // +---------------+--------------------+
    // | Variable_name | Value              |
    // +---------------+--------------------+
    // | Ssl_cipher    | DHE-RSA-AES256-SHA | (or null)
    // +---------------+--------------------+

    std::unique_ptr<MySQLSession::ResultRow> result(mysql_->query_one("show status like 'ssl_cipher'"));
    if (!result || result->size() != 2 || strcasecmp((*result)[0], "ssl_cipher"))
      throw std::runtime_error("Error reading 'ssl_cipher' status variable");

    // if ssl_cipher is empty, it means the connection is unencrypted
    if ((*result)[1] &&
        (*result)[1][0]) {
      return true;  // connection is encrypted
    } else {
      std::cerr << "WARNING: The MySQL server does not have SSL configured and "
                   "metadata used by the router may be transmitted unencrypted." << std::endl;
      return false; // connection is unencrypted
    }
  } catch (std::exception &e) {
    std::cerr << "Failed determining if metadata connection uses SSL: " << e.what() << std::endl;
    throw std::runtime_error(e.what());
  }
}

void ConfigGenerator::init(const std::string &server_url, const std::map<std::string, std::string> &bootstrap_options) {
  // Setup connection timeout
  int connection_timeout_ = 5;
  std::string uri;

  // check options
  if (bootstrap_options.find("base-port") != bootstrap_options.end()) {
    char *end = NULL;
    const char *tmp = bootstrap_options.at("base-port").c_str();
    int base_port = static_cast<int>(std::strtol(tmp, &end, 10));
    int max_base_port = (kMaxTCPPortNumber - kAllocatedTCPPortCount + 1);
    if (base_port <= 0 || base_port > max_base_port || end != tmp + strlen(tmp)) {
      throw std::runtime_error("Invalid base-port number " +
          bootstrap_options.at("base-port") +
          "; please pick a value between 1 and "+std::to_string((max_base_port)));
    }
  }
  if (bootstrap_options.find("bind-address") != bootstrap_options.end()) {
    auto address = bootstrap_options.at("bind-address");
    TCPAddress tmp(address, 1);
    if (!tmp.is_valid()) {
      throw std::runtime_error("Invalid bind-address value " + address);
    }
  }

  const std::string default_schema = "mysql://";
  // Extract connection information from the bootstrap server URL.
  if (server_url.compare(0, default_schema.size(), default_schema) != 0) {
    uri = default_schema + server_url;
  } else {
    uri = server_url;
  }

  URI u;
  try {
    // don't allow rootless URIs (mailto:foo@...) which would collide with the schema-less
    // URIs are allow too: root:pw@host
    u = URIParser::parse(uri, false);
  } catch (const mysqlrouter::URIError &e) {
    throw std::runtime_error(e.what());
  }

  // query, fragment and path should all be empty
  if (!u.fragment.empty()) {
    throw std::runtime_error("the bootstrap URI contains a #fragement, but shouldn't");
  }
  if (!u.query.empty()) {
    throw std::runtime_error("the bootstrap URI contains a ?query, but shouldn't");
  }
  if (!u.path.empty()) {
    throw std::runtime_error("the bootstrap URI contains a /path, but shouldn't");
  }

  if (u.username.empty()) {
    u.username = "root";
  }
  // we need to prompt for the password
  if (u.password.empty()) {
    u.password = prompt_password(
      "Please enter MySQL password for "+u.username);
  }

  std::string socket_name;

  // easier to just use .at() and ask for forgiveness, then useing .find() + []
  try {
    socket_name = bootstrap_options.at("bootstrap_socket");
  } catch (const std::out_of_range &e) {
    socket_name = "";
  }

  if (socket_name.size() > 0) {
    // enforce host == "localhost" if a socket is used to avoid ambiguity with the possible hostname
    if (u.host != "localhost") {
      throw std::runtime_error("--bootstrap-socket given, but --bootstrap option contains a non-'localhost' hostname: " + u.host);
    }
  } else {
    // setup localhost address.
    u.host = (u.host == "localhost" ? "127.0.0.1" : u.host);
  }

  UniquePtr<MySQLSession> s(DIM::instance().new_MySQLSession());
  try
  {
    set_ssl_options(s.get(), bootstrap_options);

    s->connect(u.host, u.port, u.username, u.password, socket_name, "", connection_timeout_);
  } catch (MySQLSession::Error &e) {
    std::stringstream err;
    err << "Unable to connect to the metadata server: " << e.what();
    throw std::runtime_error(err.str());
  }
  mysql_deleter_ = s.get_deleter(); // \.
  init(s.release());                //  > TODO get rid of mysql_* variables
  mysql_owned_ = true;              // /       (replace by DIM semantics)
}

ConfigGenerator::~ConfigGenerator() {
  if (mysql_owned_)
    mysql_deleter_(mysql_);
}

void ConfigGenerator::bootstrap_system_deployment(const std::string &config_file_path,
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::string> &default_paths,
    const std::string &keyring_file_path,
    const std::string &keyring_master_key_file) {
  auto options(user_options);
  bool quiet = user_options.find("quiet") != user_options.end();
  mysql_harness::Path _config_file_path(config_file_path);

  std::string router_name;
  if (user_options.find("name") != user_options.end()) {
    router_name = user_options.at("name");
    if (!is_valid_name(router_name))
      throw std::runtime_error("Router name '" + router_name + "' contains invalid characters.");
    if (router_name.length() > kMaxRouterNameLength)
      throw std::runtime_error("Router name '" + router_name + "' too long (max " + std::to_string(kMaxRouterNameLength) + ").");
  }
  if (router_name.empty())
    router_name = kSystemRouterName;

  if (user_options.find("socketsdir") == user_options.end())
    options["socketsdir"] = "/tmp";

  // (re-)bootstrap the instance
  UniquePtr<Ofstream> config_file = DIM::instance().new_Ofstream();
  config_file->open(config_file_path + ".tmp");
  if (config_file->fail()) {
    throw std::runtime_error("Could not open " + config_file_path + ".tmp for writing: " + get_strerror(errno));
  }
  bootstrap_deployment(*config_file, _config_file_path,
    router_name, options, default_paths,
    keyring_file_path, keyring_master_key_file,
    false);
  config_file->close();

  if (backup_config_file_if_different(config_file_path, config_file_path + ".tmp", options)) {
    if (!quiet)
      std::cout << "\nExisting configurations backed up to " << config_file_path + ".bak" << "\n";
  }

  // rename the .tmp file to the final file
  if (mysqlrouter::rename_file((config_file_path + ".tmp").c_str(), config_file_path.c_str()) != 0) {
    //log_error("Error renaming %s.tmp to %s: %s", config_file_path.c_str(),
    //  config_file_path.c_str(), get_strerror(errno));
    throw std::runtime_error("Could not save configuration file to final location");
  }
  mysql_harness::make_file_private(config_file_path);
  set_file_owner(options, config_file_path);
}

static bool is_directory_empty(mysql_harness::Directory dir) {
  for (auto di = dir.begin(); di != dir.end(); ++di) {
    std::string name = (*di).basename().str();
    if (name != "." && name != "..")
      return false;
  }
  return true;
}

/**
 * Create a self-contained deployment of the Router in a directory.
 */
void ConfigGenerator::bootstrap_directory_deployment(const std::string &directory,
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::string> &default_paths,
    const std::string &default_keyring_file_name,
    const std::string &keyring_master_key_file) {
  bool force = user_options.find("force") != user_options.end();
  bool quiet = user_options.find("quiet") != user_options.end();
  mysql_harness::Path path(directory);
  mysql_harness::Path config_file_path;
  std::string router_name;
  AutoCleaner auto_clean;

  if (user_options.find("name") != user_options.end()) {
    if ((router_name = user_options.at("name")) == kSystemRouterName)
      throw std::runtime_error("Router name '" + kSystemRouterName + "' is reserved");
    if (!is_valid_name(router_name))
      throw std::runtime_error("Router name '"+router_name+"' contains invalid characters.");
    if (router_name.length() > kMaxRouterNameLength)
      throw std::runtime_error("Router name '"+router_name+"' too long (max "+std::to_string(kMaxRouterNameLength)+").");
  }

  if (!path.exists()) {
    if (mkdir(directory.c_str(), kStrictDirectoryPerm) < 0) {
      std::cerr << "Cannot create directory " << directory << ": " << get_strerror(errno) << "\n";
#ifndef _WIN32
      if (errno == EACCES || errno == EPERM) {
        std::cerr << "This may be caused by insufficient rights or AppArmor settings.\n";
        std::cerr << "If you have AppArmor enabled try adding full path to the output directory in the mysqlrouter profile file:\n";
        std::cerr << "/etc/apparmor.d/usr.bin.mysqlrouter\n\n";
        std::cerr << "Example:\n\n";
        std::cerr << "  /path/to/your/output/dir rw,\n";
        std::cerr << "  /path/to/your/output/dir/** rw,\n";
      }
#endif
      throw std::runtime_error("Could not create deployment directory");
    }
    auto_clean.add_directory_delete(directory, true);
  }

  if (!Path(directory).is_directory()) {
    throw std::runtime_error("Can't use " + directory + " for bootstrap, it is not directory.");
  }

  set_file_owner(user_options, directory);

  path = path.real_path();
  config_file_path = path.join(mysql_harness::Path("mysqlrouter.conf"));
  if (!config_file_path.exists() && !force && !is_directory_empty(path)) {
    std::cerr << "Directory " << directory << " already contains files\n";
    throw std::runtime_error("Directory already exits");
  }

  std::map<std::string, std::string> options(user_options);

  const std::vector<std::tuple<std::string, std::string, bool>> directories {
    //              option name   dir_name      mkdir
    std::make_tuple("logdir",     "log",        true),
    std::make_tuple("rundir",     "run",        true),
    std::make_tuple("datadir",    "data",       true),
    std::make_tuple("socketsdir", "", false),
  };

  for (const auto &dir: directories) {
    const auto& option_name = std::get<0>(dir);
    const auto& dir_name = std::get<1>(dir);
    const auto& do_mkdir = std::get<2>(dir);

    if (user_options.find(option_name) == user_options.end()) {
      if (dir_name.empty()) {
        options[option_name] = path.str();
      } else {
        options[option_name] = path.join(dir_name).str();
      }
    }
    if (do_mkdir) {
      if (mkdir(options[option_name].c_str(), kStrictDirectoryPerm) < 0) {
        if (errno != EEXIST) {
          std::cerr << "Cannot create directory " << options[option_name] << ": " << get_strerror(errno) << "\n";
          throw std::runtime_error("Could not create " + option_name + "directory");
        }
      } else {
        auto_clean.add_directory_delete(options[option_name]);
      }
    }

    // sets the directory owner if the directory exists and --user provided
    set_file_owner(options, options[option_name]);
  }

  // (re-)bootstrap the instance
  std::ofstream config_file;
  config_file.open(config_file_path.str()+".tmp");
  if (config_file.fail()) {
    throw std::runtime_error("Could not open "+config_file_path.str()+".tmp for writing: "+get_strerror(errno));
  }
  auto_clean.add_file_delete(config_file_path.str()+".tmp");

  std::string keyring_path = mysql_harness::Path(options["datadir"]).
      real_path().join(default_keyring_file_name).str();

  std::string keyring_master_key_path = keyring_master_key_file.empty() ?
      "" : path.real_path().join(keyring_master_key_file).str();

  bootstrap_deployment(config_file, config_file_path, router_name, options, default_paths,
                       keyring_path, keyring_master_key_path,
                       true);
  config_file.close();

  if (backup_config_file_if_different(config_file_path, config_file_path.str() + ".tmp", options)) {
    if (!quiet)
      std::cout << "\nExisting configurations backed up to " << config_file_path.str()+".bak" << "\n";
  }

  // rename the .tmp file to the final file
  if (mysqlrouter::rename_file((config_file_path.str() + ".tmp").c_str(), config_file_path.c_str()) != 0) {
    //log_error("Error renaming %s.tmp to %s: %s", config_file_path.c_str(),
    //  config_file_path.c_str(), get_strerror(errno));
    throw std::runtime_error("Could not move configuration file '" +
      config_file_path.str() + ".tmp' to final location: "
       + mysqlrouter::get_last_error());
  }

  mysql_harness::make_file_private(config_file_path.str());
  set_file_owner(options, config_file_path.str());
  // create start/stop scripts
  create_start_scripts(path.str(), keyring_master_key_file.empty(), options);

#ifndef _WIN32
  // If we are running with --user option we need to check if the user will have access
  // to the directory where the bootstrap output files were created.
  // It may not have access if it does not have search right to any of the directories
  // on the path. We do this by switching to the --user and trying to open the config file.
  if ( options.find("user") != options.end()) {
    std::string &user_name = options.at("user");

    set_user(user_name);
    bool user_has_access{false};
    {
      std::ifstream conf_file;
      conf_file.open(config_file_path.str());
      user_has_access = !conf_file.fail();
    }
    // switch back to root, this is needed to clean up the files in case
    // the user can't access them and we are failing the bootstrap
    set_user("root");

    if (!user_has_access) {
      throw std::runtime_error("Could not access the config file as user '" + user_name
                               + "' after the bootstrap in the directory " + directory + " : "
                               + get_strerror(errno));
    }
  }
#endif

  auto_clean.clear();
}

ConfigGenerator::Options ConfigGenerator::fill_options(
    bool multi_master,
    const std::map<std::string, std::string> &user_options) {
  std::string bind_address{"0.0.0.0"};
  bool use_sockets = false;
  bool skip_tcp = false;
  bool skip_classic_protocol = false;
  bool skip_x_protocol = false;
  int base_port = 0;
  if (user_options.find("base-port") != user_options.end()) {
    char *end = NULL;
    const char *tmp = user_options.at("base-port").c_str();
    base_port = static_cast<int>(std::strtol(tmp, &end, 10));
    int max_base_port = (kMaxTCPPortNumber - kAllocatedTCPPortCount + 1);
    if (base_port <= 0 || base_port > max_base_port || end != tmp + strlen(tmp)) {
      throw std::runtime_error("Invalid base-port number " +
          user_options.at("base-port") +
          "; please pick a value lower than "+std::to_string((max_base_port)));
    }
  }
  if (user_options.find("use-sockets") != user_options.end()) {
    use_sockets = true;
  }
  if (user_options.find("skip-tcp") != user_options.end()) {
    skip_tcp = true;
  }
  ConfigGenerator::Options options;
  options.multi_master = multi_master;
  if (user_options.find("bind-address") != user_options.end()) {
    auto address = user_options.at("bind-address");
    TCPAddress tmp(address, 1);
    if (!tmp.is_valid()) {
      throw std::runtime_error("Invalid bind-address value " + address);
    }
    options.bind_address = address;
  }
  if (!skip_classic_protocol) {
    if (use_sockets) {
      options.rw_endpoint.socket = kRWSocketName;
      if (!multi_master)
        options.ro_endpoint.socket = kROSocketName;
    }
    if (!skip_tcp) {
      options.rw_endpoint.port = base_port == 0 ? kDefaultRWPort : base_port++;
      if (!multi_master)
        options.ro_endpoint.port = base_port == 0 ? kDefaultROPort : base_port++;
    }
  }
  if (!skip_x_protocol) {
    if (use_sockets) {
      options.rw_x_endpoint.socket = kRWXSocketName;
      if (!multi_master)
        options.ro_x_endpoint.socket = kROXSocketName;
    }
    if (!skip_tcp) {
      options.rw_x_endpoint.port = base_port == 0 ? kDefaultRWXPort : base_port++;
      if (!multi_master)
        options.ro_x_endpoint.port = base_port == 0 ? kDefaultROXPort : base_port++;
    }
  }
  if (user_options.find("logdir") != user_options.end())
    options.override_logdir = user_options.at("logdir");
  if (user_options.find("rundir") != user_options.end())
    options.override_rundir = user_options.at("rundir");
  if (user_options.find("datadir") != user_options.end())
    options.override_datadir = user_options.at("datadir");
  if (user_options.find("socketsdir") != user_options.end())
    options.socketsdir = user_options.at("socketsdir");

  options.ssl_options.mode = get_opt(user_options, "ssl_mode", "");
  options.ssl_options.cipher = get_opt(user_options, "ssl_cipher", "");
  options.ssl_options.tls_version = get_opt(user_options, "tls_version", "");
  options.ssl_options.ca = get_opt(user_options, "ssl_ca", "");
  options.ssl_options.capath = get_opt(user_options, "ssl_capath", "");
  options.ssl_options.crl = get_opt(user_options, "ssl_crl", "");
  options.ssl_options.crlpath = get_opt(user_options, "ssl_crlpath", "");

  return options;
}

namespace {

unsigned get_password_retries(const std::map<std::string, std::string> &user_options) {
  if (user_options.find("password-retries") == user_options.end()) {
    return kDefaultPasswordRetries;
  }

  char *end = NULL;
  const char *tmp = user_options.at("password-retries").c_str();
  unsigned result = static_cast<unsigned>(std::strtoul(tmp, &end, 10));
  if (result == 0 || result > kMaxPasswordRetries || end != tmp + strlen(tmp)) {
    throw std::runtime_error("Invalid password-retries value '" +
        user_options.at("password-retries") +
        "'; please pick a value from 1 to " + std::to_string((kMaxPasswordRetries)));
  }

  return result;
}

std::string compute_password_hash(const std::string &password) {
  uint8_t hash_stage1[SHA1_HASH_SIZE];
  my_sha1::compute_sha1_hash(hash_stage1, password.c_str(), password.length());
  uint8_t hash_stage2[SHA1_HASH_SIZE];
  my_sha1::compute_sha1_hash(hash_stage2, (const char *) hash_stage1, SHA1_HASH_SIZE);

  std::stringstream ss;
  ss << "*";
  ss << std::hex << std::setfill('0') << std::uppercase;
  for (unsigned i = 0; i < SHA1_HASH_SIZE; ++i) {
    ss << std::setw(2) << (int)hash_stage2[i];
  }

  return ss.str();
}

}

void ConfigGenerator::bootstrap_deployment(std::ostream &config_file,
    const mysql_harness::Path &config_file_path, const std::string &router_name,
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::string> &default_paths,
    const std::string &keyring_file,
    const std::string &keyring_master_key_file,
    bool directory_deployment) {
  std::string primary_cluster_name;
  std::string primary_replicaset_servers;
  std::string primary_replicaset_name;
  bool multi_master = false;
  bool force = user_options.find("force") != user_options.end();
  bool quiet = user_options.find("quiet") != user_options.end();
  uint32_t router_id = 0;
  std::string username;
  AutoCleaner auto_clean;
  using RandomGen = mysql_harness::RandomGeneratorInterface;
  RandomGen& rg = mysql_harness::DIM::instance().get_RandomGenerator();

  if (!keyring_master_key_file.empty())
    auto_clean.add_file_revert(keyring_master_key_file);
  init_keyring_file(keyring_file, keyring_master_key_file);
  set_file_owner(user_options, keyring_file);
  set_file_owner(user_options, keyring_master_key_file);

  fetch_bootstrap_servers(
    primary_replicaset_servers,
    primary_cluster_name, primary_replicaset_name,
    multi_master);

  if (config_file_path.exists()) {
    std::tie(router_id, username) = get_router_id_and_name_from_config(config_file_path.str(),
                                               primary_cluster_name, force);
  }

  if (!quiet) {
    if (router_id > 0) {
      std::cout << "\nReconfiguring";
    } else {
      std::cout << "\nBootstrapping";
    }
    if (directory_deployment) {
      std::cout << " MySQL Router instance at " + config_file_path.dirname().str() + "...\n";
    } else {
      std::cout << " system MySQL Router instance...\n";
    }
  }
  MySQLSession::Transaction transaction(mysql_);
  MySQLInnoDBClusterMetadata metadata(mysql_);

  // if reconfiguration
  if (router_id > 0) {
    std::string attributes;
    // if router data is valid
    try {
      metadata.check_router_id(router_id);
    } catch (std::exception &e) {
      std::cerr << "WARNING: " << e.what() << "\n";
      // TODO: abort here and suggest --force to force reconfiguration?
      router_id = 0;
      username.clear();
    }
  }
  // router not registered yet (or router_id was invalid)
  if (router_id == 0) {
    assert(username.empty());
    try {
      router_id = metadata.register_router(router_name, force);
      if (router_id > kMaxRouterId)
        throw std::runtime_error("router_id (" + std::to_string(router_id)
            + ") exceeded max allowable value (" + std::to_string(kMaxRouterId) + ")");
      username = "mysql_router" + std::to_string(router_id) + "_"
          + rg.generate_identifier(kNumRandomChars, RandomGen::AlphabetDigits|RandomGen::AlphabetLowercase);
    } catch (MySQLSession::Error &e) {
      if (e.code() == 1062) { // duplicate key
        throw std::runtime_error(
            "It appears that a router instance named '" + router_name +
            "' has been previously configured in this host. If that instance"
            " no longer exists, use the --force option to overwrite it."
        );
      }
      throw std::runtime_error(std::string("While registering router instance in metadata server: ")+e.what());
    }
  }

  // Create or recreate the account used by this router instance to access
  // metadata server
  assert(router_id);
  assert(!username.empty());
  std::string password = create_account(user_options, username);

  {
    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
    keyring->store(username, kKeyringAttributePassword, password);
    try {
      mysql_harness::flush_keyring();
    } catch (std::exception &e) {
      throw std::runtime_error(std::string("Error storing encrypted password to disk: ")+e.what());
    }
  }

  Options options(fill_options(multi_master, user_options));
  options.keyring_file_path = keyring_file;
  options.keyring_master_key_file_path = keyring_master_key_file;
  metadata.update_router_info(router_id, options);

#ifndef _WIN32
  /* Currently at this point the logger is not yet initialized but while bootstraping
   * with the --user=<user> option we need to create a log file and chown it to the <user>.
   * Otherwise when the router gets launched later (not bootstrap) with the same --user=<user>
   * option, the user might not have right to the logging directory.
   */
  assert(default_paths.find("logging_folder") != default_paths.end());
  std::string logdir = (!options.override_logdir.empty()) ? options.override_logdir :
                                                            default_paths.at("logging_folder");
  if (!logdir.empty()) {
    auto log_path = mysql_harness::Path::make_path(logdir, "mysqlrouter", "log");
    auto log_file = log_path.str();
    std::fstream f;
    f.open(log_file, std::ios::out);
    set_file_owner(user_options, log_file);
  }
#endif

  auto system_username = (user_options.find("user") != user_options.end()) ?  user_options.at("user") : "";

  // generate the new config file
  create_config(config_file, router_id, router_name, system_username,
                primary_replicaset_servers,
                primary_cluster_name,
                primary_replicaset_name,
                username,
                options,
                !quiet);

  transaction.commit();
  auto_clean.clear();
}

void ConfigGenerator::init_keyring_file(const std::string &keyring_file,
        const std::string &keyring_master_key_file) {
  if (keyring_master_key_file.empty()) {
    std::string master_key;
#ifdef _WIN32
    // When no master key file is provided, console interaction is required to provide a master password. Since console interaction is not available when
    // run as service, throw an error to abort.
    if (mysqlrouter::is_running_as_service()) {
      std::string msg = "Cannot run router in Windows a service without a master key file. Please run MySQL Router from the command line (instead of as a service) to create a master keyring file.";
      mysqlrouter::write_windows_event_log(msg);
      throw std::runtime_error(msg);
    }
#endif
    if (mysql_harness::Path(keyring_file).exists()) {
      master_key = prompt_password("Please provide the encryption key for key file at "+keyring_file);
      if (master_key.length() > mysql_harness::kMaxKeyringKeyLength)
        throw std::runtime_error("Encryption key is too long");
    } else {
      std::cout
        << "MySQL Router needs to create a InnoDB cluster metadata client account.\n"
        << "To allow secure storage of its password, please provide an encryption key.\n\n";
    again:
      master_key = prompt_password("Please provide an encryption key");
      if (master_key.empty()) {
        throw std::runtime_error("Keyring encryption key must not be blank");
      } else if (master_key.length() > mysql_harness::kMaxKeyringKeyLength) {
        throw std::runtime_error("Encryption key is too long");
      } else {
        std::string confirm = prompt_password("Please confirm encryption key");
        if (confirm != master_key) {
          std::cout << "Entered keys do not match. Please try again.\n";
          goto again;
        }
      }
    }
    mysql_harness::init_keyring_with_key(keyring_file, master_key, true);
  } else {
    try {
      mysql_harness::init_keyring(keyring_file, keyring_master_key_file, true);
    } catch (mysql_harness::invalid_master_keyfile &) {
      throw mysql_harness::invalid_master_keyfile("Invalid master key file "+keyring_master_key_file);
    }
  }
}

void ConfigGenerator::fetch_bootstrap_servers(
  std::string &bootstrap_servers,
  std::string &metadata_cluster,
  std::string &metadata_replicaset,
  bool &multi_master) {

  std::ostringstream query;

  // Query the name of the replicaset, the servers in the replicaset and the
  // router credentials using the URL of a server in the replicaset.
  query << "SELECT "
    "F.cluster_name, "
    "R.replicaset_name, "
    "R.topology_type, "
    "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic')) "
    "FROM "
    "mysql_innodb_cluster_metadata.clusters AS F, "
    "mysql_innodb_cluster_metadata.instances AS I, "
    "mysql_innodb_cluster_metadata.replicasets AS R "
    "WHERE "
    "R.replicaset_id = "
    "(SELECT replicaset_id FROM mysql_innodb_cluster_metadata.instances WHERE "
      "mysql_server_uuid = @@server_uuid)"
    "AND "
    "I.replicaset_id = R.replicaset_id "
    "AND "
    "R.cluster_id = F.cluster_id";

  metadata_cluster = "";
  metadata_replicaset = "";
  bootstrap_servers = "";
  try {
    mysql_->query(query.str(),
        [&metadata_cluster, &metadata_replicaset, &bootstrap_servers,
          &multi_master]
          (const std::vector<const char*> &row)->bool {
      if (metadata_cluster == "") {
        metadata_cluster = get_string(row[0]);
      } else if (metadata_cluster != get_string(row[0])) {
        // metadata with more than 1 replicaset not currently supported
        throw std::runtime_error("Metadata contains more than one cluster");
      }
      if (metadata_replicaset == "") {
        metadata_replicaset = get_string(row[1]);
      } else if (metadata_replicaset != get_string(row[1])) {
        // metadata with more than 1 replicaset not currently supported
        throw std::runtime_error("Metadata contains more than one replica-set");
      }
      if (bootstrap_servers != "")
        bootstrap_servers += ",";
      if (row[2]) {
        if (strcmp(row[2], "mm") == 0)
          multi_master = true;
        else if (strcmp(row[2], "pm") == 0)
          multi_master = false;
        else
          throw std::runtime_error("Unknown topology type in metadata: "
                                   + std::string(row[2]));
      }
      bootstrap_servers += "mysql://" + get_string(row[3]);
      return true;
    });
  } catch (MySQLSession::Error &e) {
    // log_error("MySQL error: %s (%u)", e.what(), e.code());
    // log_error("    Failed query: %s", query.str().c_str());
    throw std::runtime_error(std::string("Error querying metadata: ") + e.what());
  }
  if (metadata_cluster.empty())
    throw std::runtime_error("No clusters defined in metadata server");
}

std::string g_program_name;

#ifdef _WIN32
// This is only for Windows
static std::string find_plugin_path() {
  char szPath[MAX_PATH];
  if (GetModuleFileName(NULL, szPath, sizeof(szPath)) != 0) {
    mysql_harness::Path mypath(szPath);
    mysql_harness::Path mypath2(mypath.dirname().dirname());
    mypath2.append("lib");
    return std::string(mypath2.str());
  }
  throw std::logic_error("Could not find own installation directory");
}
#endif

static std::string find_executable_path() {
#ifdef _WIN32
  // the bin folder is not usually in the path, just the lib folder
  char szPath[MAX_PATH];
  if (GetModuleFileName(NULL, szPath, sizeof(szPath)) != 0)
  {
    char *pc = szPath - 1;
    while (*++pc)
      if (*pc == '\\') *pc = '/';
    return std::string(szPath);
  }
#else
  if (g_program_name.find('/') != std::string::npos) {
    char *tmp = realpath(g_program_name.c_str(), NULL);
    std::string path(tmp);
    free(tmp);
    return path;
  } else {
    std::string path(std::getenv("PATH"));
    char *last = NULL;
    char *p = strtok_r(&path[0], ":", &last);
    while (p) {
      if (*p && p[strlen(p)-1] == '/')
        p[strlen(p)-1] = 0;
      std::string tmp(std::string(p)+"/"+g_program_name);
      if (access(tmp.c_str(), R_OK|X_OK) == 0) {
        return tmp;
      }
      p = strtok_r(NULL, ":", &last);
    }
  }
#endif
  throw std::logic_error("Could not find own installation directory");
}

std::string ConfigGenerator::endpoint_option(const Options &options,
                                             const Options::Endpoint &ep) {
  std::string r;
  if (ep.port > 0) {
    auto bind_address = (!options.bind_address.empty()) ? options.bind_address : "0.0.0.0";
    r.append("bind_address=" + bind_address + "\n");
    r.append("bind_port=" + std::to_string(ep.port));
  }
  if (!ep.socket.empty()) {
    if (!r.empty())
      r.append("\n");
    r.append("socket=" + options.socketsdir + "/" + ep.socket);
  }
  return r;
}


static std::string option_line(const std::string &key, const std::string &value) {
  if (!value.empty()) {
    return key + "=" + value + "\n";
  }
  return "";
}

void ConfigGenerator::create_config(std::ostream &cfp,
                                    uint32_t router_id,
                                    const std::string &router_name,
                                    const std::string &system_username,
                                    const std::string &bootstrap_server_addresses,
                                    const std::string &metadata_cluster,
                                    const std::string &metadata_replicaset,
                                    const std::string &username,
                                    const Options &options,
                                    bool print_configs) {
  cfp << "# File automatically generated during MySQL Router bootstrap\n";

  cfp << "[DEFAULT]\n";
  if (!router_name.empty())
    cfp << "name=" << router_name << "\n";
  if (!system_username.empty())
    cfp << "user=" << system_username << "\n";
  if (!options.override_logdir.empty())
    cfp << "logging_folder=" << options.override_logdir << "\n";
  if (!options.override_rundir.empty())
    cfp << "runtime_folder=" << options.override_rundir << "\n";
  if (!options.override_datadir.empty())
    cfp << "data_folder=" << options.override_datadir << "\n";
  if (!options.keyring_file_path.empty())
    cfp << "keyring_path=" << options.keyring_file_path << "\n";
  if (!options.keyring_master_key_file_path.empty())
    cfp << "master_key_path=" << options.keyring_master_key_file_path << "\n";

  const std::string metadata_key = metadata_cluster;
  cfp << "\n"
      << "[logger]\n"
      << "level = INFO\n"
      << "\n"
      << "[metadata_cache:" << metadata_key << "]\n"
      << "router_id=" << router_id << "\n"
      << "bootstrap_server_addresses=" << bootstrap_server_addresses << "\n"
      << "user=" << username << "\n"
      << "metadata_cluster=" << metadata_cluster << "\n"
      << "ttl=" << kDefaultTTL << "\n";

  // SSL options
  cfp << option_line("ssl_mode", options.ssl_options.mode);
  cfp << option_line("ssl_cipher", options.ssl_options.cipher);
  cfp << option_line("tls_version", options.ssl_options.tls_version);
  cfp << option_line("ssl_ca", options.ssl_options.ca);
  cfp << option_line("ssl_capath", options.ssl_options.capath);
  cfp << option_line("ssl_crl", options.ssl_options.crl);
  cfp << option_line("ssl_crlpath", options.ssl_options.crlpath);
  // Note: we don't write cert and key because
  // creating router accounts with REQUIRE X509 is not yet supported.
  // The cert and key options passed to bootstrap if for the bootstrap
  // connection itself.
  cfp << "\n";

  const std::string fast_router_key = metadata_key+"_"+metadata_replicaset;
  if (options.rw_endpoint) {
    cfp
      << "[routing:" << fast_router_key << "_rw]\n"
      << endpoint_option(options, options.rw_endpoint) << "\n"
      << "destinations=metadata-cache://" << metadata_key << "/"
          << metadata_replicaset << "?role=PRIMARY\n"
      << "mode=read-write\n"
      << "protocol=classic\n"
      << "\n";
  }
  if (options.ro_endpoint) {
    cfp
      << "[routing:" << fast_router_key << "_ro]\n"
      << endpoint_option(options, options.ro_endpoint) << "\n"
      << "destinations=metadata-cache://" << metadata_key << "/"
          << metadata_replicaset << "?role=SECONDARY\n"
      << "mode=read-only\n"
      << "protocol=classic\n"
      << "\n";
  }
  if (options.rw_x_endpoint) {
    cfp
      << "[routing:" << fast_router_key << "_x_rw]\n"
      << endpoint_option(options, options.rw_x_endpoint) << "\n"
      << "destinations=metadata-cache://" << metadata_key << "/"
          << metadata_replicaset << "?role=PRIMARY\n"
      << "mode=read-write\n"
      << "protocol=x\n"
      << "\n";
  }
  if (options.ro_x_endpoint) {
    cfp
      << "[routing:" << fast_router_key << "_x_ro]\n"
      << endpoint_option(options, options.ro_x_endpoint) << "\n"
      << "destinations=metadata-cache://" << metadata_key << "/"
          << metadata_replicaset << "?role=SECONDARY\n"
      << "mode=read-only\n"
      << "protocol=x\n"
      << "\n";
  }
  cfp.flush();

  if (print_configs) {
    std::cout
      << "MySQL Router " << ((router_name.empty() || router_name == kSystemRouterName) ? "" : "'"+router_name+"'")
          << " has now been configured for the InnoDB cluster '" << metadata_cluster << "'" << (options.multi_master ? " (multi-master)" : "") << ".\n"
      << "\n"
      << "The following connection information can be used to connect to the cluster.\n"
      << "\n";
    if (options.rw_endpoint || options.ro_endpoint) {
      std::cout
        << "Classic MySQL protocol connections to cluster '" << metadata_cluster << "':\n";
      if (options.rw_endpoint.port > 0)
        std::cout << "- Read/Write Connections: localhost:" << options.rw_endpoint.port << "\n";
      if (!options.rw_endpoint.socket.empty())
        std::cout << "- Read/Write Connections: " << options.socketsdir + "/" + options.rw_endpoint.socket << "\n";
      if (options.ro_endpoint.port > 0)
        std::cout << "- Read/Only Connections: localhost:" << options.ro_endpoint.port << "\n";
      if (!options.ro_endpoint.socket.empty())
        std::cout << "- Read/Only Connections: " << options.socketsdir + "/" + options.ro_endpoint.socket << "\n";
      std::cout << "\n";
    }
    if (options.rw_x_endpoint || options.ro_x_endpoint) {
      std::cout
        << "X protocol connections to cluster '" << metadata_cluster << "':\n";
      if (options.rw_x_endpoint.port > 0)
        std::cout << "- Read/Write Connections: localhost:" << options.rw_x_endpoint.port << "\n";
      if (!options.rw_x_endpoint.socket.empty())
        std::cout << "- Read/Write Connections: " << options.socketsdir + "/" + options.rw_x_endpoint.socket << "\n";
      if (options.ro_x_endpoint.port > 0)
        std::cout << "- Read/Only Connections: localhost:" << options.ro_x_endpoint.port << "\n";
      if (!options.ro_x_endpoint.socket.empty())
        std::cout << "- Read/Only Connections: " << options.socketsdir + "/" + options.ro_x_endpoint.socket << "\n";
    }
  }
}

std::string ConfigGenerator::create_account(const std::map<std::string, std::string> &user_options,
                                            const std::string &username) {
  using RandomGen = mysql_harness::RandomGeneratorInterface;
  RandomGen& rg = mysql_harness::DIM::instance().get_RandomGenerator();

  const bool force_password_validation = user_options.find("force-password-validation") != user_options.end();
  std::string password;
  bool account_created = false;
  unsigned retries = get_password_retries(user_options);
  if (!force_password_validation) {
    // 1) Try to create an account using mysql_native_password with the hashed password
    //    to avoid validate_password verification.
    password = rg.generate_strong_password(kMetadataServerPasswordLength);
    const std::string hashed_password = compute_password_hash(password);
    try {
      create_account(username, hashed_password, true /*password_hashed*/);
      account_created = true;
    }
    catch (const plugin_not_loaded&) {
      // fallback to 2)
    }
  }

  // 2) If 1) failed because of the missing mysql_native_password plugin,
  //    or "-force-password-validation" parameter has being used
  //    try to create an account using the password directly
  if (!account_created) {
    while (true) {
      password = rg.generate_strong_password(kMetadataServerPasswordLength);

      try {
        create_account(username, password);
      }
      catch(const password_too_weak& e) {
        if (--retries == 0) {
          // 3) If 2) failed issue an error suggesting the change to validate_password rules
          std::stringstream err_msg;
          err_msg << "Error creating user account: " << e.what() << std::endl
                  << " Try to decrease the validate_password rules and try the operation again.";
          throw std::runtime_error(err_msg.str());
        }
        // generated password does not satisfy the current policy requirements.
        // we do our best to generate strong password but with the validate_password
        // plugin, the user can set very strong or unusual requirements that we are not able to
        // predict so we just retry several times hoping to meet the requirements with the next
        // generated password.
        continue;
      }

      // no expception while creating account, we are good to continue
      break;
    }
  }

  return password;
}

/*
  Create MySQL account for this instance of the router in the target cluster.

  The account will have access to the cluster metadata and to the
  replication_group_members table of the performance_schema.
  Note that this assumes that the metadata schema is stored in the destinations
  cluster and that there is only one replicaset in it.
 */
void ConfigGenerator::create_account(const std::string &username,
                                     const std::string &password,
                                     bool password_hashed) {
  std::string host = "%";
  /*
  Ideally, we create a single account for the specific host that the router is
  running on. But that has several problems in real world, including:
  - if you're configuring on localhost ref to metadata server, the router will
  think it's in localhost and thus it will need 2 accounts: user@localhost
  and user@public_ip... further, there could be more than 1 IP for the host,
  which (like lan IP, localhost, internet IP, VPN IP, IPv6 etc). We don't know
  which ones are needed, so either we need to automatically create all of those
  or have some very complicated and unreliable logic.
  - using hostname is not reliable, because not every place will have name
  resolution availble
  - using IP (even if we can detect it correctly) will not work if IP is not
  static

  So we create the accoun@%, to make things simple. The account has limited
  privileges and is specific to the router instance (password not shared), so
  that shouldn't be a issue.
  {
    // try to find out what's our public IP address
    std::unique_ptr<MySQLSession::ResultRow> row(mysql_->query_one(
        "SELECT s.ip"
        " FROM performance_schema.socket_instances s JOIN performance_schema.threads t"
        "   ON s.thread_id = t.thread_id"
        " WHERE t.processlist_id = connection_id()"));
    if (row) {
      //std::cout << "our host is visible to the MySQL server as IP " << (*row)[0] << "\n";
      host = (*row)[0];
      std::string::size_type sep = host.rfind(':');
      if (sep != std::string::npos) {
        host = host.substr(sep+1);
      }
    } else {
      host = get_my_hostname();
    }
  }*/
  const std::string account = username + "@" + mysql_->quote(host);
  // log_info("Creating account %s", account.c_str());

  const std::string create_user = "CREATE USER " + account + " IDENTIFIED "
      + (password_hashed ? "WITH mysql_native_password AS " : "BY ")
      + mysql_->quote(password);


  const std::vector<std::string> queries{
    "DROP USER IF EXISTS " + account,
    create_user,
    "GRANT SELECT ON mysql_innodb_cluster_metadata.* TO " + account,
    "GRANT SELECT ON performance_schema.replication_group_members TO " + account,
    "GRANT SELECT ON performance_schema.replication_group_member_stats TO " + account
  };

  for (auto &q : queries) {
    try {
      mysql_->execute(q);
    } catch (MySQLSession::Error &e) {
      // log_error("%s: executing query: %s", e.what(), q.c_str());
      try {
        mysql_->execute("ROLLBACK");
      } catch (...) {
        // log_error("Could not rollback transaction explicitly.");
      }
      std::string err_msg = std::string("Error creating MySQL account for router: ") + e.what();
      if (e.code() == 1819) { // password does not satisfy the current policy requirements
        throw password_too_weak(err_msg);
      }
      if (e.code()== 1524) { // plugin not loaded
        throw plugin_not_loaded(err_msg);
      }

      throw std::runtime_error(err_msg);
    }
  }
}

/**
 * Get router_id name values associated with a metadata_cache configuration for
 * the given cluster_name.
 *
 * The lookup is done through the metadata_cluster option inside the
 * metadata_cache section.
 */
std::pair<uint32_t, std::string> ConfigGenerator::get_router_id_and_name_from_config(
    const std::string &config_file_path,
    const std::string &cluster_name,
    bool forcing_overwrite) {
  mysql_harness::Path path(config_file_path);
  std::string existing_cluster;
  if (path.exists()) {
    mysql_harness::Config config(mysql_harness::Config::allow_keys);
    config.read(path);
    mysql_harness::Config::SectionList sections;
    if (config.has_any("metadata_cache")) {
      sections = config.get("metadata_cache");
    } else {
      return std::make_pair(0, "");
    }
    if (sections.size() > 1) {
      throw std::runtime_error("Bootstrapping of Router with multiple metadata_cache sections not supported");
    }
    for (auto const &section : sections) {
      if (section->has("metadata_cluster")) {
        existing_cluster = section->get("metadata_cluster");
        if (existing_cluster == cluster_name) {
          // get router_id
          if (! section->has("router_id")) {
            std::cerr << "WARNING: router_id not set for cluster "+cluster_name+"\n";
            return std::make_pair(0, "");
          }
          std::string tmp = section->get("router_id");
          char *end;
          unsigned long r = std::strtoul(tmp.c_str(), &end, 10);
          if (end == tmp.c_str() || errno == ERANGE) {
              throw std::runtime_error("Invalid router_id '"+tmp
                  +"' for cluster '"+cluster_name+"' in "+config_file_path);
          }

          // get username, example: user=mysql_router4_kot8tcepf3kn
          if (! section->has("user")) {
            std::cerr << "WARNING: user not set for cluster "+cluster_name+"\n";
            return std::make_pair(0, "");
          }
          std::string user = section->get("user");

          // return results
          return std::make_pair(static_cast<uint32_t>(r), user);
        }
      }
    }
  }
  if (!forcing_overwrite) {
    std::string msg;
    msg += "The given Router instance is already configured for a cluster named '"+existing_cluster+"'.\n";
    msg += "If you'd like to replace it, please use the --force configuration option.";
    //XXX when multiple-clusters is supported, also suggest --add
    throw std::runtime_error(msg);
  }
  return std::make_pair(0, "");
}

void ConfigGenerator::create_start_scripts(const std::string &directory,
                                           bool interactive_master_key,
                                           const std::map<std::string, std::string> &options) {
#ifdef _WIN32
  std::ofstream script;
  std::string script_path = directory + "/start.ps1";

  script.open(script_path);
  if (script.fail()) {
    throw std::runtime_error("Could not open " + script_path + " for writing: " + get_strerror(errno));
  }
  script << "$env:path += \";" << find_plugin_path() << "\"" << std::endl;
  script << "[Environment]::SetEnvironmentVariable(\"ROUTER_PID\"," << "\"" << directory << "\\" << "mysqlrouter.pid\", \"Process\")" << std::endl;
  script << "Start-Process \"" << find_executable_path() << "\" \" -c " << directory << "/mysqlrouter.conf\"" << " -WindowStyle Hidden" << std::endl;
  script.close();

  script_path = directory + "/stop.ps1";
  script.open(script_path);
  if (script.fail()) {
    throw std::runtime_error("Could not open " + script_path + " for writing: " + get_strerror(errno));
  }
  script << "$filename = [Environment]::GetEnvironmentVariable(\"ROUTER_PID\", \"Process\")" << std::endl;
  script << "If(Test-Path $filename) {" << std::endl;
  script << "  $mypid = [IO.File]::ReadAllText($filename)" << std::endl;
  script << "  Stop-Process -Id $mypid" << std::endl;
  script << "  [IO.File]::Delete($filename)" << std::endl;
  script << "}" << std::endl;
  script << "else { Write-Host \"Error when trying to stop mysqlrouter process\" }" << std::endl;
  script.close();


#else
  std::ofstream script;
  std::string script_path = directory+"/start.sh";

  std::string owner_name;
  bool change_owner = options.find("user") != options.end();
  if (change_owner) {
    owner_name = options.at("user");
  }

  script.open(script_path);
  if (script.fail()) {
    throw std::runtime_error("Could not open "+script_path+" for writing: "+get_strerror(errno));
  }
  script << "#!/bin/bash\n";
  script << "basedir=" << directory << "\n";
  if (interactive_master_key) {
    // prompt for password if master_key_path is not set
    script << "old_stty=`stty -g`\n";
    script << "stty -echo\n";
    script << "echo -n 'Encryption key for router keyring:'\n";
    script << "read password\n";
    script << "stty $old_stty\n";
    script << "echo $password | ";
  }
  script << (change_owner ? "sudo " : "")
         <<"ROUTER_PID=$basedir/mysqlrouter.pid "
         << find_executable_path() << " -c " << "$basedir/mysqlrouter.conf "
         << (change_owner ? std::string("--user=" + owner_name) : "")
         << "&\n";
  script << "disown %-\n";
  script.close();
  if (::chmod(script_path.c_str(), kStrictDirectoryPerm) < 0) {
    std::cerr << "Could not change permissions for " << script_path << ": " << get_strerror(errno) << "\n";
  }
  set_file_owner(options, script_path);

  script_path = directory+"/stop.sh";
  script.open(script_path);
  if (script.fail()) {
    throw std::runtime_error("Could not open " + script_path + " for writing: " + get_strerror(errno));
  }
  script << "if [ -f " + directory + "/mysqlrouter.pid ]; then\n";
  script << "  kill -HUP `cat " + directory + "/mysqlrouter.pid`\n";
  script << "  rm -f " << directory + "/mysqlrouter.pid\n";
  script << "fi\n";
  script.close();
  if (::chmod(script_path.c_str(), kStrictDirectoryPerm) < 0) {
    std::cerr << "Could not change permissions for " << script_path << ": " << get_strerror(errno) << "\n";
  }
  set_file_owner(options, script_path);
#endif
}

static bool files_equal(const std::string &f1, const std::string &f2) {
  std::ifstream if1(f1);
  std::ifstream if2(f2);

  if1.seekg(0, if1.end);
  std::streamoff fsize = if1.tellg();
  if1.seekg(0, if1.beg);

  if2.seekg(0, if2.end);
  if (fsize != if2.tellg())
    return false;
  if2.seekg(0, if2.beg);

  std::string data1, data2;
  data1.resize(static_cast<size_t>(fsize));
  data2.resize(static_cast<size_t>(fsize));

  if1.read(&data1[0], static_cast<std::streamsize>(fsize));
  if2.read(&data2[0], static_cast<std::streamsize>(fsize));

  return data1 == data2;
}

bool ConfigGenerator::backup_config_file_if_different(const mysql_harness::Path &config_path,
                                                      const std::string &new_file_path,
                                                      const std::map<std::string, std::string> &options) {
  if (config_path.exists()) {
    // if the old and new config files are the same, don't bother with a backup
    if (!files_equal(config_path.str(), new_file_path)) {
      std::string file_name = config_path.str()+".bak";
      copy_file(config_path.str(), file_name);
      mysql_harness::make_file_private(file_name);
      set_file_owner(options, file_name);
      return true;
    }
  }
  return false;
}

void ConfigGenerator::set_file_owner(const std::map<std::string, std::string> &options,
                                     const std::string &file_path)
{
#ifndef _WIN32
  bool change_owner = (options.count("user") != 0) && (!options.at("user").empty());
  if (change_owner) {
    auto username = options.at("user");
    auto user_info = check_user(username, true, sys_user_operations_);
    if (user_info != nullptr) {
      mysqlrouter::set_owner_if_file_exists(file_path, username, user_info, sys_user_operations_);
    }
  }
#endif
}
