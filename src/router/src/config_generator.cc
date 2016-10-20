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

#include "keyring/keyring_manager.h"
#include "config_generator.h"
#include "mysqlrouter/uri.h"
#include "common.h"
#include "filesystem.h"
#include "config_parser.h"
#include "common.h"
#include "mysqlrouter/mysql_session.h"
#include "utils_sqlstring.h"
#include "rapidjson/rapidjson.h"
#include "utils.h"
// #include "logger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#ifdef _WIN32
#include <Winsock2.h>
#include <string.h>
#include <io.h>
#define strtok_r strtok_s
#define strcasecmp _stricmp
#else
#include <termios.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#ifndef __APPLE__
#include <ifaddrs.h>
#include <net/if.h>
#endif
#endif
#include <cstring>

static const int kDefaultRWPort = 6446;
static const int kDefaultROPort = 6447;
static const char *kRWSocketName = "mysql.sock";
static const char *kROSocketName = "mysqlro.sock";

static const int kDefaultRWXPort = 64460;
static const int kDefaultROXPort = 64470;
static const char *kRWXSocketName = "mysqlx.sock";
static const char *kROXSocketName = "mysqlxro.sock";

static const std::string kSystemRouterName = "system";

static const int kMetadataServerPasswordLength = 16;

static const char *kKeyringAttributePassword = "password";

using mysql_harness::get_strerror;
using mysqlrouter::sqlstring;
using mysqlrouter::MySQLSession;

static std::string prompt_password(const std::string &prompt);

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


static std::string get_my_hostname() {
#if defined(_WIN32) || defined(__APPLE__) || defined(__FreeBSD__)
  char hostname[1024] = {0};
  if (gethostname(hostname, sizeof(hostname)) < 0) {
    char msg[1024];
#ifndef WIN32
    (void)strerror_r(errno, msg, sizeof(msg));
#else
    (void)strerror_s(msg, sizeof(msg), errno);
#endif
    // log_error("Could not get hostname: %s", msg);
    throw std::runtime_error("Could not get local hostname");
  }
  return hostname;
}
#else
  struct ifaddrs *ifa, *ifap;
  char buf[INET6_ADDRSTRLEN] = {0};
  int ret = -1, family;
  socklen_t addrlen;

  if (getifaddrs(&ifa) != 0 || !ifa)
    throw std::runtime_error("Could not get local host address: " + std::string(strerror(errno)));
  for (ifap = ifa; ifap != NULL; ifap = ifap->ifa_next) {
    if ((ifap->ifa_addr == NULL) || (ifap->ifa_flags & IFF_LOOPBACK) || (!(ifap->ifa_flags & IFF_UP)))
      continue;
    family = ifap->ifa_addr->sa_family;
    if (family != AF_INET && family != AF_INET6)
      continue;
    if (family == AF_INET6) {
      struct sockaddr_in6 *sin6;

      sin6 = (struct sockaddr_in6 *)ifap->ifa_addr;
      if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) || IN6_IS_ADDR_MC_LINKLOCAL(&sin6->sin6_addr))
        continue;
    }
    addrlen = static_cast<socklen_t>((family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
    ret = getnameinfo(ifap->ifa_addr, addrlen, buf,
        static_cast<socklen_t>(sizeof(buf)), NULL, 0, NI_NAMEREQD);
  }
  if (ret != EAI_NONAME && ret != 0)
    throw std::runtime_error("Could not get local host address: " + std::string(gai_strerror(ret)));
  return buf;
}
#endif

static std::string generate_password(int password_length) {
  std::random_device rd;
  std::string pwd;
  const char *alphabet = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ~@#%$^&*()-_=+]}[{|;:.>,</?";
  std::uniform_int_distribution<unsigned long> dist(0, strlen(alphabet) - 1);

  for (int i = 0; i < password_length; i++)
    pwd += alphabet[dist(rd)];

  return pwd;
}


class MySQLInnoDBClusterMetadata {
public:
  MySQLInnoDBClusterMetadata(MySQLSession *mysql)
  : mysql_(mysql) {}

  void check_router_id(uint32_t router_id);
  uint32_t register_router(const std::string &router_name, bool overwrite);
  void update_router_info(uint32_t router_id,
                          const ConfigGenerator::Options &options);
private:
  MySQLSession *mysql_;
};


void MySQLInnoDBClusterMetadata::check_router_id(uint32_t router_id) {
  // query metadata for this router_id
  sqlstring query("SELECT h.host_id, h.host_name"
                  " FROM mysql_innodb_cluster_metadata.routers r"
                  " JOIN mysql_innodb_cluster_metadata.hosts h"
                  "    ON r.host_id = h.host_id"
                  " WHERE r.router_id = ?");
  query << router_id << sqlstring::end;
  std::unique_ptr<MySQLSession::ResultRow> row(mysql_->query_one(query));
  if (!row) {
    //log_warning("router_id %u not in metadata", router_id);
    throw std::runtime_error("router_id "+std::to_string(router_id)+" not found in metadata");
  }
  std::string hostname = get_my_hostname();
  if ((*row)[1] && strcasecmp((*row)[1], hostname.c_str()) == 0) {
    return;
  }
  //log_warning("router_id %u maps to an instance at hostname %s, while this hostname is %s",
  //                router_id, row[1], hostname.c_str());

  // if the host doesn't match, we force a new router_id to be generated
  throw std::runtime_error("router_id " + std::to_string(router_id)
      + " is associated with a different host ("+(*row)[1]+" vs "+hostname+")");
}

inline std::string str(const ConfigGenerator::Options::Endpoint &ep) {
  if (ep.port > 0)
    return std::to_string(ep.port);
  else if (!ep.socket.empty())
    return ep.socket;
  else
    return "null";
}

void MySQLInnoDBClusterMetadata::update_router_info(uint32_t router_id,
    const ConfigGenerator::Options &options) {
  sqlstring query("UPDATE mysql_innodb_cluster_metadata.routers"
                  " SET attributes = "
                  "   JSON_SET(JSON_SET(JSON_SET(JSON_SET(attributes,"
                  "    'RWEndpoint', ?),"
                  "    'ROEndpoint', ?),"
                  "    'RWXEndpoint', ?),"
                  "    'ROXEndpoint', ?)"
                  " WHERE router_id = ?");
  query << str(options.rw_endpoint);
  query << str(options.ro_endpoint);
  query << str(options.rw_x_endpoint);
  query << str(options.ro_x_endpoint);
  query << router_id;
  query << sqlstring::end;

  mysql_->execute(query);
}

uint32_t MySQLInnoDBClusterMetadata::register_router(
    const std::string &router_name, bool overwrite) {
  uint32_t host_id;
  std::string hostname = get_my_hostname();
  // check if the host already exists in the metadata schema and if so, get
  // our host_id.. if it doesn't, insert it and get the host_id
  sqlstring query("SELECT host_id, host_name, ip_address"
                   " FROM mysql_innodb_cluster_metadata.hosts"
                   " WHERE host_name = ?"
                   " LIMIT 1");
  query << hostname << sqlstring::end;
  {
    std::unique_ptr<MySQLSession::ResultRow> row(mysql_->query_one(query));
    if (!row) {
      // host is not known to the metadata, register it
      query = sqlstring("INSERT INTO mysql_innodb_cluster_metadata.hosts"
                        "        (host_name, location, attributes)"
                        " VALUES (?, '', "
                        "         JSON_OBJECT('registeredFrom', 'mysql-router'))");
      query << hostname << sqlstring::end;
      mysql_->execute(query);
      host_id = static_cast<uint32_t>(mysql_->last_insert_id());
      // log_info("host_id for local host %s newly registered as %u",
      //        hostname.c_str(), host_id);
    } else {
      host_id = static_cast<uint32_t>(std::strtoul((*row)[0], NULL, 10));
      // log_info("host_id for local host %s already registered as %u",
      //        hostname.c_str(), host_id);
    }
  }
  // now insert the router and get the router id
  query = sqlstring("INSERT INTO mysql_innodb_cluster_metadata.routers"
                    "        (host_id, router_name)"
                    " VALUES (?, ?)");
  // log_info("Router instance '%s' registered with id %u", router_name.c_str(), router_id);
  query << host_id << router_name << sqlstring::end;
  try {
    mysql_->execute(query);
  } catch (MySQLSession::Error &e) {
    if (e.code() == 1062 && overwrite)  {
      //log_warning("Replacing instance %s (host_id %i) of router",
      //            router_name.c_str(), host_id);
      query = sqlstring("SELECT router_id FROM mysql_innodb_cluster_metadata.routers"
                        " WHERE host_id = ? AND router_name = ?");
      query << host_id << router_name << sqlstring::end;
      std::unique_ptr<MySQLSession::ResultRow> row(mysql_->query_one(query));
      if (row) {
        return static_cast<uint32_t>(std::stoul((*row)[0]));
      }
      throw;
    }
  }
  return static_cast<uint32_t>(mysql_->last_insert_id());
}


void ConfigGenerator::init(MySQLSession *session) {
  mysql_ = session;

  // check that the server we're talking to actually looks like a metadata server
  // 1- it must have the mysql_innodb_cluster_metadata schema
  auto *result = session->query_one("SHOW SCHEMAS LIKE 'mysql_innodb_cluster_metadata'");
  if (!result) {
    throw std::runtime_error("The given server does not seem to contain metadata for a MySQL InnoDB cluster");
  }
  delete result;
}

void ConfigGenerator::init(const std::string &server_url) {
  // Setup connection timeout
  int connection_timeout_ = 5;
  // Extract connection information from the bootstrap server URL.
  std::string normalized_url(server_url);
  if (normalized_url.find("//") == std::string::npos) {
    normalized_url = "mysql://" + normalized_url;
  }
  mysqlrouter::URI u(normalized_url);

  if (u.username.empty()) {
    u.username = "root";
  }
  // setup localhost address.
  u.host = (u.host == "localhost" ? "127.0.0.1" : u.host);

  // we need to prompt for the password
  if (u.password.empty()) {
    u.password = prompt_password(
      "Please enter MySQL password for "+u.username);
  }

  std::unique_ptr<MySQLSession> s(new MySQLSession());
  try
  {
    s->connect(u.host, u.port, u.username, u.password, connection_timeout_);
  } catch (MySQLSession::Error &e) {
    std::stringstream err;
    err << "Unable to connect to the metadata server: " << e.what();
    throw std::runtime_error(err.str());
  }
  init(s.release());
  mysql_owned_ = true;
}

ConfigGenerator::~ConfigGenerator() {
  if (mysql_owned_)
    delete mysql_;
}

// Moves a file without assuming the dest is already created.
void my_rename(const std::string& src, const std::string& dest)
{
  try {
    std::ifstream is(src, std::ios_base::binary | std::ios_base::in);
    if (!is)
      throw std::runtime_error("Could not save configuration file to final location: " + std::to_string(errno));
    std::ofstream os(dest, std::ios_base::binary | std::ios_base::out);
    is.seekg(0, is.end);
    int size = static_cast<int>(is.tellg());
    std::unique_ptr<char> buf(new char[size]);
    is.seekg(0, is.beg);
    is.read(buf.get(), size);
    os.write(buf.get(), size);
    is.close();
    os.close();
    mysqlrouter::delete_file(src);
  }
  catch (...) {
    throw std::runtime_error("Could not save configuration file to final location" + std::to_string(errno));
  }
}

void ConfigGenerator::bootstrap_system_deployment(const std::string &config_file_path,
    const std::map<std::string, std::string> &user_options,
    const std::string &keyring_file_path,
    const std::string &keyring_master_key_file) {
  if (user_options.find("name") != user_options.end()) {
    throw std::runtime_error("Router instance name can only be specified with --directory option");
  }
  // check if the config file already exists and if it has a router_id
  uint32_t router_id = get_router_id_from_config_file(config_file_path);
  if (router_id > 0) {
    std::cout << "Reconfiguring system wide MySQL Router instance...\n\n";
  } else {
    std::cout << "Bootstrapping system wide MySQL Router instance...\n\n";
  }
  auto options(user_options);

  if (user_options.find("socketsdir") == user_options.end())
    options["socketsdir"] = "/tmp";

  // (re-)bootstrap the instance
  std::ofstream config_file;
  config_file.open(config_file_path+".tmp");
  if (config_file.fail()) {
    throw std::runtime_error("Could not open "+config_file_path+".tmp for writing: "+get_strerror(errno));
  }
  init_keyring_file(keyring_file_path, keyring_master_key_file);
  bootstrap_deployment(config_file, router_id, kSystemRouterName, options,
                       keyring_file_path, keyring_master_key_file);
  config_file.close();

  // rename the .tmp file to the final file
  mysql_harness::make_file_private(config_file_path);
  my_rename(config_file_path + ".tmp", config_file_path);
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
    const std::string &default_keyring_file_name,
    const std::string &keyring_master_key_file) {
  uint32_t router_id = 0;
  bool force = user_options.find("force") != user_options.end();
  mysql_harness::Path path(directory);
  mysql_harness::Path config_file_path;
  std::string router_name;
  if (user_options.find("name") != user_options.end()) {
    if ((router_name = user_options.at("name")) == kSystemRouterName)
      throw std::runtime_error("Router name " + kSystemRouterName + " is reserved");
    for (auto c : router_name) {
      if (c == '\'' || c == '`' || c == '"' || c == ':' || c == '[' || c == ']')
        throw std::runtime_error("Router name "+router_name+" contains invalid characters");
    }
  } else {
    throw std::runtime_error("Please use the --name option to give a label for this router instance");
  }
  if (path.exists()) {
    path = path.real_path();
    config_file_path = path.join(mysql_harness::Path("mysqlrouter.conf"));
    if (config_file_path.exists()) {
      router_id = get_router_id_from_config_file(config_file_path.str(),
                                                 router_name);
    } else if (!force && !is_directory_empty(path)) {
      std::cerr << "Directory " << directory << " already contains files\n";
      throw std::runtime_error("Directory already exits");
    }
  } else {
    if (mysqlrouter::mkdir(directory.c_str(), 0700) < 0) {
      std::cerr << "Cannot create directory " << directory << ": " << mysql_harness::get_strerror(errno) << "\n";
      throw std::runtime_error("Could not create deployment directory");
    }
    path = path.real_path();
    config_file_path = path.join(mysql_harness::Path("mysqlrouter.conf"));
  }
  std::map<std::string, std::string> options(user_options);
  if (router_id > 0) {
    std::cout << "Reconfiguring MySQL Router instance at " + directory + "...\n\n";
  } else {
    std::cout << "Bootstrapping MySQL Router instance at " + directory + "...\n\n";
  }
  if (user_options.find("logdir") == user_options.end())
    options["logdir"] = path.join("log").str();
  if (user_options.find("rundir") == user_options.end())
    options["rundir"] = path.join("run").str();
  if (user_options.find("socketsdir") == user_options.end())
    options["socketsdir"] = path.str();
  if (mysqlrouter::mkdir(options["logdir"].c_str(), 0700) < 0 && errno != EEXIST) {
    std::cerr << "Cannot create directory " << options["logdir"] << ": " << get_strerror(errno) << "\n";
    throw std::runtime_error("Could not create deployment directory");
  }
  if (mysqlrouter::mkdir(options["rundir"].c_str(), 0700) < 0 && errno != EEXIST) {
    std::cerr << "Cannot create directory " << options["rundir"] << ": " << get_strerror(errno) << "\n";
    throw std::runtime_error("Could not create deployment directory");
  }
  // (re-)bootstrap the instance
  std::ofstream config_file;
  config_file.open(config_file_path.str()+".tmp");
  if (config_file.fail()) {
    throw std::runtime_error("Could not open "+config_file_path.str()+".tmp for writing: "+get_strerror(errno));
  }
  try {
    std::string keyring_path = mysql_harness::Path(options["rundir"]).
        real_path().join(default_keyring_file_name).str();
    init_keyring_file(keyring_path, keyring_master_key_file);
    bootstrap_deployment(config_file, router_id, router_name, options,
                         keyring_path, keyring_master_key_file);
  } catch (...) {
    config_file.close();
    mysqlrouter::delete_file(config_file_path.str()+".tmp");
    mysqlrouter::rmdir(options["logdir"]);
    mysqlrouter::rmdir(options["rundir"]);
    throw;
  }
  config_file.close();

  // rename the .tmp file to the final file
  mysql_harness::make_file_private(config_file_path.str());
  my_rename(config_file_path.str() + ".tmp", config_file_path.str());
  // create start/stop scripts
  create_start_scripts(path.str(), keyring_master_key_file.empty());
}

ConfigGenerator::Options ConfigGenerator::fill_options(
    bool multi_master,
    const std::map<std::string, std::string> &user_options) {
  bool use_sockets = false;
  bool skip_tcp = false;
  bool skip_classic_protocol = false;
  bool skip_x_protocol = false;
  int base_port = 0;
  if (user_options.find("base-port") != user_options.end()) {
    char *end = NULL;
    const char *tmp = user_options.at("base-port").c_str();
    base_port = static_cast<int>(std::strtol(tmp, &end, 10));
    if (base_port <= 0 || base_port > 65535 || end != tmp + strlen(tmp)) {
      throw std::runtime_error("Invalid base-port value " + user_options.at("base-port"));
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
  if (user_options.find("socketsdir") != user_options.end())
    options.socketsdir = user_options.at("socketsdir");
  return options;
}

void ConfigGenerator::bootstrap_deployment(std::ostream &config_file,
    uint32_t router_id, const std::string &router_name,
    const std::map<std::string, std::string> &user_options,
    const std::string &keyring_file,
    const std::string &keyring_master_key_file) {
  std::string primary_cluster_name;
  std::string primary_replicaset_servers;
  std::string primary_replicaset_name;
  bool multi_master = false;
  bool force = user_options.find("force") != user_options.end();

  fetch_bootstrap_servers(
    primary_replicaset_servers,
    primary_cluster_name, primary_replicaset_name,
    multi_master);

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
    }
  }
  // router not registered yet (or router_id was invalid)
  if (router_id == 0) {
    try {
      router_id = metadata.register_router(router_name, force);
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

  Options options(fill_options(multi_master, user_options));
  options.keyring_file_path = keyring_file;
  options.keyring_master_key_file_path = keyring_master_key_file;

  // Create or recreate the account used by this router instance to access
  // metadata server
  std::string username = "mysql_innodb_cluster_router"+std::to_string(router_id);
  std::string password = generate_password(kMetadataServerPasswordLength);
  {
    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
    keyring->store(username, kKeyringAttributePassword, password);
    try {
      mysql_harness::flush_keyring();
    } catch (std::exception &e) {
      throw std::runtime_error(std::string("Error storing encrypted password to disk: ")+e.what());
    }
  }

  create_account(username, password);

  metadata.update_router_info(router_id, options);

  // generate the new config file
  create_config(config_file, router_id, router_name,
                primary_replicaset_servers,
                primary_cluster_name,
                primary_replicaset_name,
                username,
                options);

  transaction.commit();
}

void ConfigGenerator::init_keyring_file(const std::string &keyring_file,
        const std::string &keyring_master_key_file) {
  if (keyring_master_key_file.empty()) {
    std::string master_key;
    if (mysql_harness::Path(keyring_file).exists()) {
      master_key = prompt_password("Please provide the encryption key for key file");
    } else {
      std::cout
        << "MySQL Router needs to create a InnoDB cluster metadata client account.\n"
        << "To allow secure storage of its password, please provide an encryption key.\n"
        << "To generate a random encryption key to be stored in a local obscured file,\n"
        << "and allow the router to start without interaction, press Return to cancel\n"
        << "and use the --master-key-path option to specify a file location.\n\n";
    again:
      master_key = prompt_password("Please provide an encryption key");
      if (master_key.empty()) {
        throw std::runtime_error("cancelled");
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
    mysql_harness::init_keyring(keyring_file, keyring_master_key_file, true);
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
        [this, &metadata_cluster, &metadata_replicaset, &bootstrap_servers,
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
        return path;
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
  if (ep.port > 0)
    r.append("bind_port=" + std::to_string(ep.port));
  if (!ep.socket.empty()) {
    if (!r.empty())
      r.append("\n");
    r.append("socket=" + options.socketsdir + "/" + ep.socket);
  }
  return r;
}


void ConfigGenerator::create_config(
  std::ostream &cfp,
  uint32_t router_id,
  const std::string &router_name,
  const std::string &bootstrap_server_addresses,
  const std::string &metadata_cluster,
  const std::string &metadata_replicaset,
  const std::string &username,
  const Options &options) {

  cfp << "# File automatically generated during MySQL Router bootstrap\n";

  cfp << "[DEFAULT]\n";
  if (!options.override_logdir.empty())
    cfp << "logging_folder=" << options.override_logdir << "\n";
  if (!options.override_rundir.empty())
    cfp << "runtime_folder=" << options.override_rundir << "\n";
  if (!options.keyring_file_path.empty())
    cfp << "keyring_path=" << options.keyring_file_path << "\n";
  if (!options.keyring_master_key_file_path.empty())
    cfp << "master_key_path=" << options.keyring_master_key_file_path << "\n";

  cfp << "\n"
      << "[logger]\n"
      << "level = INFO\n"
      << "\n"
      << "[metadata_cache" << ((router_name.empty() || router_name == kSystemRouterName) ? "" : ":"+router_name) << "]\n"
      << "router_id=" << router_id << "\n"
      << "bootstrap_server_addresses=" << bootstrap_server_addresses << "\n"
      << "user=" << username << "\n"
      << "metadata_cluster=" << metadata_cluster << "\n"
      << "ttl=300" << "\n"
      << "metadata_replicaset=" << metadata_replicaset << "\n"
      << "\n";
  std::string router_tag;
  std::string router_key = metadata_replicaset;
  if (!router_name.empty() && router_name != kSystemRouterName) {
    router_key = router_name+"_"+metadata_replicaset;
    router_tag = router_name;
  }
  if (options.rw_endpoint) {
    cfp
      << "[routing:" << router_key << "_rw]\n"
      << endpoint_option(options, options.rw_endpoint) << "\n"
      << "destinations=metadata-cache://" << router_tag << "/"
          << metadata_replicaset << "?role=PRIMARY\n"
      << "mode=read-write\n"
      << "\n";
  }
  if (options.ro_endpoint) {
    cfp
      << "[routing:" << router_key << "_ro]\n"
      << endpoint_option(options, options.ro_endpoint) << "\n"
      << "destinations=metadata-cache://" << router_tag << "/"
          << metadata_replicaset << "?role=SECONDARY\n"
      << "mode=read-only\n"
      << "\n";
  }
  if (options.rw_x_endpoint) {
    cfp
      << "[routing:" << router_key << "_x_rw]\n"
      << endpoint_option(options, options.rw_x_endpoint) << "\n"
      << "destinations=metadata-cache://" << router_tag << "/"
          << metadata_replicaset << "?role=PRIMARY\n"
      << "mode=read-write\n"
      << "protocol=x\n"
      << "\n";
  }
  if (options.ro_x_endpoint) {
    cfp
      << "[routing:" << router_key << "_x_ro]\n"
      << endpoint_option(options, options.ro_x_endpoint) << "\n"
      << "destinations=metadata-cache://" << router_tag << "/"
          << metadata_replicaset << "?role=SECONDARY\n"
      << "mode=read-only\n"
      << "protocol=x\n"
      << "\n";
  }
  cfp.flush();

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
    else if (!options.rw_endpoint.socket.empty())
      std::cout << "- Read/Write Connections: " << options.socketsdir + "/" + options.rw_endpoint.socket << "\n";
    if (options.ro_endpoint.port > 0)
      std::cout << "- Read/Only Connections: localhost:" << options.ro_endpoint.port << "\n";
    else if (!options.ro_endpoint.socket.empty())
      std::cout << "- Read/Only Connections: " << options.socketsdir + "/" + options.ro_endpoint.socket << "\n";
    std::cout << "\n";
  }
  if (options.rw_x_endpoint || options.ro_x_endpoint) {
    std::cout
      << "X protocol connections to cluster '" << metadata_cluster << "':\n";
    if (options.rw_x_endpoint)
      std::cout << "- Read/Write Connections: localhost:" << options.rw_x_endpoint.port << "\n";
    else if (!options.rw_x_endpoint.socket.empty())
      std::cout << "- Read/Write Connections: " << options.socketsdir + "/" + options.rw_x_endpoint.socket << "\n";
    if (options.ro_x_endpoint)
      std::cout << "- Read/Only Connections: localhost:" << options.ro_x_endpoint.port << "\n";
    else if (!options.ro_x_endpoint.socket.empty())
      std::cout << "- Read/Only Connections: " << options.socketsdir + "/" + options.ro_x_endpoint.socket << "\n";
  }
}


/*
  Create MySQL account for this instance of the router in the target cluster.

  The account will have access to the cluster metadata and to the
  replication_group_members table of the performance_schema.
  Note that this assumes that the metadata schema is stored in the destinations
  cluster and that there is only one replicaset in it.
 */
void ConfigGenerator::create_account(const std::string &username,
                                     const std::string &password) {
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
  privileges and is specific to the router instance (passwrd not shared), so
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
  std::string account = username + "@" + mysql_->quote(host);
  // log_info("Creating account %s", account.c_str());

  std::vector<std::string> queries{
    "DROP USER IF EXISTS " + account,
    "CREATE USER " + account + " IDENTIFIED BY " + mysql_->quote(password),
    "GRANT SELECT ON mysql_innodb_cluster_metadata.* TO " + account,
    "GRANT SELECT ON performance_schema.replication_group_members TO " + account
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
      throw std::runtime_error(std::string("Error creating MySQL account for router: ") + e.what());
    }
  }
}

uint32_t ConfigGenerator::get_router_id_from_config_file(
    const std::string &config_file_path,
    const std::string &name) {
  mysql_harness::Path path(config_file_path);
  if (path.exists()) {
    mysql_harness::Config config(mysql_harness::Config::allow_keys);
    config.read(path);
    if (config.get("metadata_cache").size() > 1) {
      throw std::runtime_error("Bootstrapping of Router with multiple metadata_cache sections not supported");
    }
    auto section(config.get("metadata_cache", name));
    if (section.key == name && section.has("router_id")) {
      std::string tmp = section.get("router_id");
      char *end;
      unsigned long r = std::strtoul(tmp.c_str(), &end, 10);
      if (end == tmp.c_str() || errno == ERANGE) {
        // log_warning("Invalid router_id '%s' found in existing config file '%s'",
            // tmp.c_str(), config_file_path.c_str());
      } else {
        return static_cast<uint32_t>(r);
      }
    }
  }
  return 0;
}

void ConfigGenerator::create_start_scripts(const std::string &directory,
                                           bool interactive_master_key) {
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
  script << "ROUTER_PID=$basedir/mysqlrouter.pid " << find_executable_path() << " -c " << "$basedir/mysqlrouter.conf &\n";
  script << "disown %-\n";
  script.close();
  if (chmod(script_path.c_str(), 0700) < 0) {
    std::cerr << "Could not change permissions for " << script_path << ": " << get_strerror(errno) << "\n";
  }

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
  if (chmod(script_path.c_str(), 0700) < 0) {
    std::cerr << "Could not change permissions for " << script_path << ": " << get_strerror(errno) << "\n";
  }
#endif
}

/**
 * Prompt for the password that will be used for accessing the metadata
 * in the metadata servers.
 *
 * @param prompt The password prompt.
 *
 * @return a string representing the password.
 */
#ifndef _WIN32
static std::string prompt_password(const std::string &prompt) {
  struct termios console;
  tcgetattr(STDIN_FILENO, &console);

  std::cout << prompt << ": ";

  // prevent showing input
  console.c_lflag &= ~(uint)ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &console);

  std::string result;
  std::getline(std::cin, result);

  // reset
  console.c_lflag |= ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &console);

  std::cout << std::endl;
  return result;
}
#else
static std::string prompt_password(const std::string &prompt) {

  std::cout << prompt << ": ";

  // prevent showing input
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode;
  GetConsoleMode(hStdin, &mode);
  mode &= ~ENABLE_ECHO_INPUT;
  SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));

  std::string result;
  std::getline(std::cin, result);

  // reset
  SetConsoleMode(hStdin, mode);

  std::cout << std::endl;
  return result;
}
#endif
