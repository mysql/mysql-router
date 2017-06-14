/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"
#include "keyring/keyring_manager.h"
#include "router_app.h"
#include "config_generator.h"
#include "mysql_session.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "common.h"

#ifndef _WIN32
#ifdef __sun
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#include <unistd.h>
#include <signal.h>
const char dir_sep = '/';
const std::string path_sep = ":";
#else
#include <windows.h>
#include <process.h>
#define getpid _getpid
#include "mysqlrouter/windows/password_vault.h"
#include <string.h>
#include <io.h>
#define strtok_r strtok_s
const char dir_sep = '\\';
const std::string path_sep = ";";
#endif



using std::string;
using std::vector;
using mysql_harness::get_strerror;
using mysqlrouter::string_format;
using mysqlrouter::substitute_envvar;
using mysqlrouter::wrap_string;
using mysqlrouter::SysUserOperationsBase;
using mysqlrouter::SysUserOperations;

static const char *kDefaultKeyringFileName = "keyring";

static std::string find_full_path(const std::string &argv0) {
#ifdef _WIN32
  // the bin folder is not usually in the path, just the lib folder
  char szPath[MAX_PATH];
  if (GetModuleFileName(NULL, szPath, sizeof(szPath)) != 0)
    return std::string(szPath);
#else
  mysql_harness::Path p_argv0(argv0);
  // Path normalizes '\' to '/'
  if (p_argv0.str().find('/') != std::string::npos) {
    // Path is either absolute or relative to the current working dir, so
    // we can use realpath() to find the full absolute path
    mysql_harness::Path path2(p_argv0.real_path());
    const char *tmp = path2.c_str();
    std::string path(tmp);
    return path;
  } else {
    // Program was found via PATH lookup by the shell, so we
    // try to find the program in one of the PATH dirs
    std::string path(std::getenv("PATH"));
    char *last = NULL;
    char *p = strtok_r(&path[0], path_sep.c_str(), &last);
    while (p) {
      std::string tmp(std::string(p)+dir_sep+argv0);
      if(mysqlrouter::my_check_access(tmp)) {
        mysql_harness::Path path1(tmp.c_str());
        mysql_harness::Path path2(path1.real_path());
        return path2.str();
      }
      p = strtok_r(NULL, path_sep.c_str(), &last);
    }
  }
#endif
  throw std::logic_error("Could not find own installation directory");
}

static std::string substitute_variable(const std::string &s,
                                       const std::string &name,
                                       const std::string &value) {
  std::string r(s);
  std::string::size_type p;
  while ((p = r.find(name)) != std::string::npos) {
    std::string tmp(r.substr(0, p));
    tmp.append(value);
    tmp.append(r.substr(p+name.size()));
    r = tmp;
  }
  mysqlrouter::substitute_envvar(r);
  mysql_harness::Path path(r);
  if (path.exists())
    return path.real_path().str();
  else
    return r;
}

static inline void set_signal_handlers() {
#ifndef _WIN32
  // until we have proper signal handling we need at least
  // mask out broken pipe to prevent terminating the router
  // if the receiving end closes the socket while the router
  // writes to it
  signal(SIGPIPE, SIG_IGN);
#endif
}

// Check if the value is valid regular filename and if it is add to the vector,
// if it is not throw an exception
static void check_and_add_conf(std::vector<string> &configs,
                               const std::string& value) throw(std::runtime_error) {
  mysql_harness::Path cfg_file_path;
  try {
    cfg_file_path = mysql_harness::Path(value);
  } catch (const std::invalid_argument &exc) {
    throw std::runtime_error(string_format("Failed reading configuration file: %s", exc.what()));
  }

  if (cfg_file_path.is_regular()) {
    configs.push_back(cfg_file_path.real_path().str());
  } else if (cfg_file_path.is_directory()) {
    throw std::runtime_error(string_format("Expected configuration file, got directory name: %s", value.c_str()));
  } else {
    throw std::runtime_error(string_format("Failed reading configuration file: %s", value.c_str()));
  }
}

MySQLRouter::MySQLRouter(const mysql_harness::Path& origin, const vector<string>& arguments
#ifndef _WIN32
                         , SysUserOperationsBase *sys_user_operations
#endif
                         )
    : version_(MYSQL_ROUTER_VERSION_MAJOR, MYSQL_ROUTER_VERSION_MINOR, MYSQL_ROUTER_VERSION_PATCH),
      arg_handler_(), loader_(), can_start_(false),
      showing_info_(false),
      origin_(origin)
#ifndef _WIN32
    , sys_user_operations_(sys_user_operations)
#endif
{
  set_signal_handlers();
  init(arguments);
}

MySQLRouter::MySQLRouter(const int argc, char **argv
#ifndef _WIN32
  , SysUserOperationsBase *sys_user_operations
#endif
)
    : MySQLRouter(mysql_harness::Path(find_full_path(argv[0])).dirname(),
                  vector<string>({argv + 1, argv + argc})
#ifndef _WIN32
                 ,sys_user_operations
#endif
    )
{
}

// throws std::runtime_error
void MySQLRouter::parse_command_options(const vector<string>& arguments) {
  prepare_command_options();
  try {
    arg_handler_.process(arguments);
  } catch (const std::invalid_argument &exc) {
    throw std::runtime_error(exc.what());
  }
}

void MySQLRouter::init(const vector<string>& arguments) {
  set_default_config_files(CONFIG_FILES);

  parse_command_options(arguments); // throws std::runtime_error

  if (showing_info_) {
    return;
  }

  if (!bootstrap_uri_.empty()) {

#ifndef _WIN32
    // If the user does the bootstrap with superuser (uid==0) but did not provide
    // --user option let's encourage her/him to do so.
    // Otherwise [s]he will end up with the files (config, log, etc.) owned
    // by the root user and not accessible by others, which is likely not what
    // was expected. The user still can use --user=root to force using superuser.
    bool user_option = this->bootstrap_options_.count("user") != 0 ;
    bool superuser = sys_user_operations_->geteuid() == 0;

    if (superuser && !user_option) {
      std::string msg("You are bootstraping as a superuser.\n"
                      "This will make all the result files (config etc.) privately owned by the superuser.\n"
                      "Please use --user=username option to specify the user that will be running the router.\n"
                      "Use --user=root if this really should be the superuser.");

      throw std::runtime_error(msg);
    }
#endif

    bootstrap(bootstrap_uri_);
    return;
  }

  available_config_files_ = check_config_files();
  can_start_ = true;
}

void MySQLRouter::init_keyring(mysql_harness::Config &config) {
  bool needs_keyring = false;

  if (config.has_any("metadata_cache")) {
    auto metadata_caches = config.get("metadata_cache");
    for (auto &section : metadata_caches) {
      if (section->has("user")) {
        needs_keyring = true;
        break;
      }
    }
  }
  if (needs_keyring) {
    // Initialize keyring
    std::string keyring_file;
    std::string master_key_path;

    if (config.has_default("keyring_path"))
      keyring_file = config.get_default("keyring_path");
    if (config.has_default("master_key_path"))
      master_key_path = config.get_default("master_key_path");

    // fill in default keyring file path, if not set
    if (keyring_file.empty()) {
      keyring_file = substitute_variable(MYSQL_ROUTER_DATA_FOLDER,
                                         "{origin}", origin_.str());
      keyring_file = mysql_harness::Path(keyring_file).join(kDefaultKeyringFileName).str();
    }
    // if keyring master key is in a file, read from it, else read from user
    if (!master_key_path.empty()) {
      mysql_harness::init_keyring(keyring_file, master_key_path, false);
    } else {
#ifdef _WIN32
      // When no master key file is provided, console interaction is required to provide a master password. Since console interaction is not available when
      // run as service, throw an error to abort.
      if (mysqlrouter::is_running_as_service())
      {
        std::string msg = "Cannot run router in Windows a service without a master key file.";
        mysqlrouter::write_windows_event_log(msg);
        throw std::runtime_error(msg);
      }
#endif
      std::string master_key = mysqlrouter::prompt_password("Encryption key for router keyring");
      if (master_key.length() > mysql_harness::kMaxKeyringKeyLength)
        throw std::runtime_error("Encryption key is too long");
      mysql_harness::init_keyring_with_key(keyring_file, master_key, false);
    }
  }
}

static string fixpath(const string &path, const std::string &basedir) {
  if (path.empty())
    return basedir;
  if (path.compare(0, strlen("{origin}"), "{origin}") == 0)
    return path;
  if (path.find("ENV{") != std::string::npos)
    return path;
#ifdef _WIN32
  if (path[0] == '\\' || path[0] == '/' || path[1] == ':')
    return path;
  // if the path is not absolute, it must be relative to the origin
  return basedir+"\\"+path;
#else
  if (path[0] == '/')
    return path;
  // if the path is not absolute, it must be relative to the origin
  return basedir+"/"+path;
#endif
}

std::map<std::string, std::string> MySQLRouter::get_default_paths() {
  std::string basedir = mysql_harness::Path(origin_).dirname().str();

  std::map<std::string, std::string> params = {
      {"program", "mysqlrouter"},
      {"origin", origin_.str()},
      {"logging_folder", fixpath(MYSQL_ROUTER_LOGGING_FOLDER, basedir)},
      {"plugin_folder", fixpath(MYSQL_ROUTER_PLUGIN_FOLDER, basedir)},
      {"runtime_folder", fixpath(MYSQL_ROUTER_RUNTIME_FOLDER, basedir)},
      {"config_folder", fixpath(MYSQL_ROUTER_CONFIG_FOLDER, basedir)},
      {"data_folder", fixpath(MYSQL_ROUTER_DATA_FOLDER, basedir)}
  };
  // check if the executable is being ran from the install location and if not
  // set the plugin dir to a path relative to it
#ifndef _WIN32
  {
    mysql_harness::Path install_origin(fixpath(MYSQL_ROUTER_BINARY_FOLDER, basedir));
    if (!install_origin.exists() || !(install_origin.real_path() == origin_)) {
      params["plugin_folder"] = fixpath(MYSQL_ROUTER_PLUGIN_FOLDER, basedir);
    }
  }
#else
  {
    mysql_harness::Path install_origin(fixpath(MYSQL_ROUTER_BINARY_FOLDER, basedir));
    if (!install_origin.exists() || !(install_origin.real_path() == origin_)) {
      params["plugin_folder"] = origin_.dirname().join("lib").str();
    }
  }
#endif

  // resolve environment variables & relative paths
  for (auto it : params) {
    std::string &param = params.at(it.first);
    param.assign(substitute_variable(param, "{origin}", origin_.str()));
  }
  return params;
}


void MySQLRouter::start() {
  if (showing_info_ || !bootstrap_uri_.empty()) {
    // when we are showing info like --help or --version, we do not throw
    return;
  }
  if (!can_start_) {
    throw std::runtime_error("Can not start");
  }

#ifndef _WIN32
  // if the --user parameter was provided on the command line, switch
  // to the user asap before accessing the external files to check
  // that the user has rights to use them
  if (!user_cmd_line_.empty()) {
    set_user(user_cmd_line_, true, this->sys_user_operations_);
  }
#endif

  string err_msg = "Configuration error: %s.";

  // Using environment variable ROUTER_PID is a temporary solution. We will remove this
  // functionality when Harness introduces the `pid_file` option.
  auto pid_file_env = std::getenv("ROUTER_PID");
  if (pid_file_env != nullptr) {
    pid_file_path_ = pid_file_env;
    mysql_harness::Path pid_file_path(pid_file_path_);
    if (pid_file_path.is_regular()) {
      throw std::runtime_error(string_format("PID file %s found. Already running?", pid_file_path_.c_str()));
    }
  }

  auto params = get_default_paths();

  try {
    loader_ = std::unique_ptr<mysql_harness::Loader>(new mysql_harness::Loader("mysqlrouter", params));
    for (auto &&config_file: available_config_files_) {
      loader_->read(mysql_harness::Path(config_file));
    }
  } catch (const mysql_harness::syntax_error &err) {
    throw std::runtime_error(string_format(err_msg.c_str(), err.what()));
  } catch (const std::runtime_error &err) {
    throw std::runtime_error(string_format(err_msg.c_str(), err.what()));
  }

  if (!pid_file_path_.empty()) {
    auto pid = getpid();
    std::ofstream pidfile(pid_file_path_);
    if (pidfile.good()) {
      pidfile << pid << std::endl;
      pidfile.close();
      std::cout << "PID " << pid << " written to " << pid_file_path_ << std::endl;
    } else {
      throw std::runtime_error(
          string_format("Failed writing PID to %s: %s", pid_file_path_.c_str(), mysqlrouter::get_last_error(errno).c_str()));
    }
  }
  loader_->add_logger("INFO");

  std::list<mysql_harness::Config::SectionKey> plugins = loader_->available();
  if (plugins.size() < 2) {
    std::cout << "MySQL Router not configured to load or start any plugin. Exiting." << std::endl;
    return;
  }

  // there can be at most one metadata_cache section because
  // currently the router supports only one metadata_cache instance
  auto config = loader_->get_config();
  if (config.has_any("metadata_cache") && config.get("metadata_cache").size() > 1) {
    std::cout << "MySQL Router currently supports only one metadata_cache instance." << std::endl
              << "There is more than one metadata_cache section in the router configuration. Exiting." << std::endl;
    return;
  }

#ifndef _WIN32
  // --user param given on the command line has a priority over
  // the user in the configuration
  if (user_cmd_line_.empty() && config.has_default("user")) {
    set_user(config.get_default("user"), true, this->sys_user_operations_);
  }
#endif
  init_keyring(config);

  try {
    auto log_file = loader_->get_log_file();
    std::string log_path(log_file.str());
    size_t pos;
    pos = log_path.find_last_of('/');
    if (pos != std::string::npos)
      log_path.erase(pos);
    if (mysqlrouter::mkdir(log_path, mysqlrouter::kStrictDirectoryPerm) != 0)
      throw std::runtime_error("Error when creating dir '" + log_path + "': " + std::to_string(errno));
    std::cout << "Logging to " << log_file << std::endl;
  } catch (...) {
    // We are logging to console
  }
  loader_->start();
}

void MySQLRouter::set_default_config_files(const char *locations) noexcept {
  std::stringstream ss_line{locations};

  // We remove all previous entries
  default_config_files_.clear();
  std::vector<string>().swap(default_config_files_);

  for (string file; std::getline(ss_line, file, ';');) {
    bool ok = substitute_envvar(file);
    if (ok) { // if there's no placeholder in file path, this is OK too
      default_config_files_.push_back(substitute_variable(file, "{origin}",
                                                          origin_.str()));
    } else {
      // Any other problem with placeholders we ignore and don't use file
    }
  }
}

string MySQLRouter::get_version() noexcept {
  return string(MYSQL_ROUTER_VERSION);
}

string MySQLRouter::get_version_line() noexcept {
  std::ostringstream os;
  string edition{MYSQL_ROUTER_VERSION_EDITION};

  os << PACKAGE_NAME << " v" << get_version();

  os << " on " << PACKAGE_PLATFORM << " (" << (PACKAGE_ARCH_64BIT ? "64-bit" : "32-bit") << ")";

  if (!edition.empty()) {
    os << " (" << edition << ")";
  }

  return os.str();
}

vector<string> MySQLRouter::check_config_files() {
  vector<string> result;

  size_t nr_of_none_extra = 0;

  auto config_file_containers = {
    &default_config_files_,
    &config_files_,
    &extra_config_files_
  };

  auto check_file = [](const std::string &file_name) -> bool {
    std::ifstream file_check;
    file_check.open(file_name);
    return  file_check.is_open();
  };

  auto use_ini_extension = [](const std::string &file_name) -> std::string {
    auto pos = file_name.find_last_of(".conf");
    if (pos == std::string::npos || (pos != file_name.length() - 1)) {
      return std::string();
    }
    return file_name.substr(0, pos - 4) + ".ini";
  };

  std::string paths_attempted;
  for (vector<string> *vec: config_file_containers) {
    for (auto &file: *vec) {
      auto pos = std::find(result.begin(), result.end(), file);
      if (pos != result.end()) {
        throw std::runtime_error(string_format("Duplicate configuration file: %s.", file.c_str()));
      }
      if (check_file(file)) {
        result.push_back(file);
        if (vec != &extra_config_files_) {
          nr_of_none_extra++;
        }
        continue;
      }

      // if this is a default path we also check *.ini version to be backward compatible
      // with the previous router versions that used *.ini
      std::string file_ini;
      if (vec == &default_config_files_) {
        file_ini = use_ini_extension(file);
        if (!file_ini.empty() && check_file(file_ini)) {
          result.push_back(file_ini);
          nr_of_none_extra++;
          continue;
        }
      }

      paths_attempted.append(file).append(path_sep);
      if (!file_ini.empty())
          paths_attempted.append(file_ini).append(path_sep);
    }
  }

  // Can not have extra configuration files when we do not have other configuration files
  if (!extra_config_files_.empty() && nr_of_none_extra == 0) {
    throw std::runtime_error("Extra configuration files only work when other configuration files are available.");
  }

  if (result.empty()) {
    throw std::runtime_error("No valid configuration file available. See --help for more information (looked at paths '" + paths_attempted + "').");
  }

  return result;
}

void MySQLRouter::save_bootstrap_option_not_empty(const std::string& option_name, const std::string& save_name,
                                     const std::string& option_value) {
  if (this->bootstrap_uri_.empty())
    throw std::runtime_error("Option " + option_name + " can only be used together with -B/--bootstrap");

  if (option_value.empty())
    throw std::runtime_error("Value for option '" + option_name + "' can't be empty.");

  bootstrap_options_[save_name] = option_value;
}

void MySQLRouter::prepare_command_options() noexcept {

  // General guidelines for naming command line options:
  //
  // Option names that start with --conf are meant to affect
  // configuration only and used during bootstrap.
  // If an option affects the bootstrap process itself, it should
  // omit the --conf prefix, even if it affects both the bootstrap
  // and the configuration.

  arg_handler_.clear_options();
  arg_handler_.add_option(CmdOption::OptionNames({"-v", "--version"}), "Display version information and exit.",
                          CmdOptionValueReq::none, "", [this](const string &) {
        std::cout << this->get_version_line() << std::endl;
        this->showing_info_ = true;
      });

  arg_handler_.add_option(CmdOption::OptionNames({"-h", "--help"}), "Display this help and exit.",
                          CmdOptionValueReq::none, "", [this](const string &) {
        this->show_help();
        this->showing_info_ = true;
      });

  arg_handler_.add_option(OptionNames({"-B", "--bootstrap"}),
                          "Bootstrap and configure Router for operation with a MySQL InnoDB cluster.",
                          CmdOptionValueReq::required, "server_url",
                          [this](const string &server_url) {
        if (server_url.empty()) {
          throw std::runtime_error("Invalid value for --bootstrap/-B option");
        }
#ifndef _WIN32
        // at the point the --user option is being processed we need to know if it is a bootstrap
        // or not so we can't allow --bootstrap option to be used after the -u/--user
        if (!this->user_cmd_line_.empty()) {
          throw std::runtime_error("Option -u/--user needs to be used after the --bootstrap option");
        }
#endif
        this->bootstrap_uri_ = server_url;
      });

  arg_handler_.add_option(OptionNames({"--bootstrap-socket"}),
                          "Bootstrap and configure Router via a Unix socket",
                          CmdOptionValueReq::required, "socket_name",
                          [this](const string &socket_name) {
        if (socket_name.empty()) {
            throw std::runtime_error("Invalid value for --bootstrap-socket option");
        }

        this->save_bootstrap_option_not_empty("--bootstrap-socket", "bootstrap_socket", socket_name);
    });

  arg_handler_.add_option(OptionNames({"-d", "--directory"}),
                          "Creates a self-contained directory for a new instance of the Router. (bootstrap)",
                          CmdOptionValueReq::required, "directory",
                          [this](const string &path) {
        if (path.empty()) {
          throw std::runtime_error("Invalid value for --directory option");
        }
        this->bootstrap_directory_ = path;
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option -d/--directory can only be used together with -B/--bootstrap");
        }
      });

#ifndef _WIN32
  arg_handler_.add_option(OptionNames({"--conf-use-sockets"}),
                          "Whether to use Unix domain sockets. (bootstrap)",
                          CmdOptionValueReq::none, "",
                          [this](const string &) {
        this->bootstrap_options_["use-sockets"] = "1";
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option --conf-use-sockets can only be used together with -B/--bootstrap");
        }
      });

  arg_handler_.add_option(OptionNames({"--conf-skip-tcp"}),
                          "Whether to disable binding of a TCP port for incoming connections. (bootstrap)",
                          CmdOptionValueReq::none, "",
                          [this](const string &) {
        this->bootstrap_options_["skip-tcp"] = "1";
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option --conf-skip-tcp can only be used together with -B/--bootstrap");
        }
      });
#endif
  arg_handler_.add_option(OptionNames({"--conf-base-port"}),
                          "Base port to use for listening router ports. (bootstrap)",
                          CmdOptionValueReq::required, "port",
                          [this](const string &port) {
        this->bootstrap_options_["base-port"] = port;
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option --conf-base-port can only be used together with -B/--bootstrap");
        }
      });

  arg_handler_.add_option(OptionNames({"--conf-bind-address"}),
                          "IP address of the interface to which router's listening sockets should bind. (bootstrap)",
                          CmdOptionValueReq::required, "address",
                          [this](const string &address) {
        this->bootstrap_options_["bind-address"] = address;
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option --conf-bind-address can only be used together with -B/--bootstrap");
        }
      });

#ifndef _WIN32
  arg_handler_.add_option(OptionNames({"-u", "--user"}),
                          "Run the mysqlrouter as the user having the name user_name.",
                          CmdOptionValueReq::required, "username",
                          [this](const string &username) {
        if (this->bootstrap_uri_.empty()) {
          this->user_cmd_line_ = username;
        }
        else {
          check_user(username, true, this->sys_user_operations_);
          this->bootstrap_options_["user"] =  username;
        }
      });
#endif

  arg_handler_.add_option(OptionNames({"--name"}),
                          "Gives a symbolic name for the router instance. (bootstrap)",
                          CmdOptionValueReq::optional, "name",
                          [this](const string &name) {
        this->bootstrap_options_["name"] = name;
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option --name can only be used together with -B/--bootstrap");
        }
      });

  arg_handler_.add_option(OptionNames({"--force-password-validation"}),
                          "When autocreating database account do not use HASHED password. (bootstrap)",
                          CmdOptionValueReq::none, "",
                          [this](const string &) {
        this->bootstrap_options_["force-password-validation"] = "1";
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option --force-password-validation can only be used together with -B/--bootstrap");
        }
      });

  arg_handler_.add_option(OptionNames({"--password-retries"}),
                          "Number of the retries for generating the router's user password. (bootstrap)",
                          CmdOptionValueReq::optional, "password-retries",
                          [this](const string &retries) {
        this->bootstrap_options_["password-retries"] = retries;
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option --password-retries can only be used together with -B/--bootstrap");
        }
      });

  arg_handler_.add_option(OptionNames({"--force"}),
                          "Force reconfiguration of a possibly existing instance of the router. (bootstrap)",
                          CmdOptionValueReq::none, "",
                          [this](const string &) {
        this->bootstrap_options_["force"] = "1";
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option --force can only be used together with -B/--bootstrap");
        }
      });

  char ssl_mode_vals[128];
  char ssl_mode_desc[256];
  snprintf(ssl_mode_vals, sizeof(ssl_mode_vals), "%s|%s|%s|%s|%s",
           mysqlrouter::MySQLSession::kSslModeDisabled,
           mysqlrouter::MySQLSession::kSslModePreferred,
           mysqlrouter::MySQLSession::kSslModeRequired,
           mysqlrouter::MySQLSession::kSslModeVerifyCa,
           mysqlrouter::MySQLSession::kSslModeVerifyIdentity);
  snprintf(ssl_mode_desc, sizeof(ssl_mode_desc),
           "SSL connection mode for use during bootstrap and normal operation, when connecting to the metadata server. Analogous to --ssl-mode in mysql client. One of %s. Default = %s. (bootstrap)",
           ssl_mode_vals, mysqlrouter::MySQLSession::kSslModePreferred);

  arg_handler_.add_option(OptionNames({"--ssl-mode"}), ssl_mode_desc,
                          CmdOptionValueReq::required, "mode",
                          [this](const string &ssl_mode) {
        if (this->bootstrap_uri_.empty())
          throw std::runtime_error("Option --ssl-mode can only be used together with -B/--bootstrap");

        try {
          mysqlrouter::MySQLSession::parse_ssl_mode(ssl_mode);  // we only care if this succeeds
          bootstrap_options_["ssl_mode"] = ssl_mode;
        } catch (const std::logic_error& e) {
          throw std::runtime_error("Invalid value for --ssl-mode option");
        }
      });

  arg_handler_.add_option(OptionNames({"--ssl-cipher"}), ": separated list of SSL ciphers to allow, if SSL is enabeld.",
                          CmdOptionValueReq::required, "ciphers",
                          [this](const string &cipher) {
        this->save_bootstrap_option_not_empty("--ssl-cipher", "ssl_cipher", cipher);
      });

  arg_handler_.add_option(OptionNames({"--tls-version"}), ", separated list of TLS versions to request, if SSL is enabled.",
                          CmdOptionValueReq::required, "versions",
                          [this](const string &version) {
        this->save_bootstrap_option_not_empty("--tls-version", "tls_version", version);
      });

  arg_handler_.add_option(OptionNames({"--ssl-ca"}), "Path to SSL CA file to verify server's certificate against.",
                          CmdOptionValueReq::required, "path",
                          [this](const string &path) {
        this->save_bootstrap_option_not_empty("--ssl-ca", "ssl_ca", path);
      });

  arg_handler_.add_option(OptionNames({"--ssl-capath"}), "Path to directory containing SSL CA files to verify server's certificate against.",
                          CmdOptionValueReq::required, "directory",
                          [this](const string &path) {
        this->save_bootstrap_option_not_empty("--ssl-capath", "ssl_capath", path);
      });

  arg_handler_.add_option(OptionNames({"--ssl-crl"}), "Path to SSL CRL file to use when verifying server certificate.",
                          CmdOptionValueReq::required, "path",
                          [this](const string &path) {
        this->save_bootstrap_option_not_empty("--ssl-crl", "ssl_crl", path);
      });

  arg_handler_.add_option(OptionNames({"--ssl-crlpath"}), "Path to directory containing SSL CRL files to use when verifying server certificate.",
                          CmdOptionValueReq::required, "directory",
                          [this](const string &path) {
        this->save_bootstrap_option_not_empty("--ssl-crlpath", "ssl_crlpath", path);
      });

// 2017.01.26: Disabling this code, since it's not part of GA v2.1.2.  It should be re-enabled later
#if 0
  arg_handler_.add_option(OptionNames({"--ssl-cert"}), "Path to client SSL certificate, to be used if client certificate verification is required. Used during bootstrap only.",
                          CmdOptionValueReq::required, "path",
                          [this](const string &path) {
        if (this->bootstrap_uri_.empty())
          throw std::runtime_error("Option --ssl-cert can only be used together with -B/--bootstrap");

        bootstrap_options_["ssl_cert"] = path;
      });

  arg_handler_.add_option(OptionNames({"--ssl-key"}), "Path to private key for client SSL certificate, to be used if client certificate verification is required. Used during bootstrap only.",
                          CmdOptionValueReq::required, "path",
                          [this](const string &path) {
        if (this->bootstrap_uri_.empty())
          throw std::runtime_error("Option --ssl-key can only be used together with -B/--bootstrap");

        bootstrap_options_["ssl_key"] = path;
      });
#endif

  arg_handler_.add_option(OptionNames({"-c", "--config"}),
                          "Only read configuration from given file.",
                          CmdOptionValueReq::required, "path", [this](const string &value)
                          throw(std::runtime_error) {

        if (!config_files_.empty()) {
          throw std::runtime_error("Option -c/--config can only be used once; use -a/--extra-config instead.");
        }

        // When --config is used, no defaults shall be read
        default_config_files_.clear();
        check_and_add_conf(config_files_, value);
      });

  arg_handler_.add_option(CmdOption::OptionNames({"-a", "--extra-config"}),
                          "Read this file after configuration files are read from either "
                              "default locations or from files specified by the --config option.",
                          CmdOptionValueReq::required, "path", [this](const string &value)
                          throw(std::runtime_error) {
        check_and_add_conf(extra_config_files_, value);
      });
// These are additional Windows-specific options, added (at the time of writing) in check_service_operations().
// Grep after '--install-service' and you shall find.
#ifdef _WIN32
  arg_handler_.add_option(CmdOption::OptionNames({"--install-service"}), "Install Router as Windows service",
                          CmdOptionValueReq::none, "", [this](const string &) {/*implemented elsewhere*/});

  arg_handler_.add_option(CmdOption::OptionNames({"--install-service-manual"}), "Install Router as Windows service, manually",
                          CmdOptionValueReq::none, "", [this](const string &) {/*implemented elsewhere*/});

  arg_handler_.add_option(CmdOption::OptionNames({"--remove-service"}), "Remove Router from Windows services",
                          CmdOptionValueReq::none, "", [this](const string &) {/*implemented elsewhere*/});

  arg_handler_.add_option(CmdOption::OptionNames({"--service"}), "Start Router as Windows service",
                          CmdOptionValueReq::none, "", [this](const string &) {/*implemented elsewhere*/});

  arg_handler_.add_option(CmdOption::OptionNames({ "--update-credentials-section" }), "Updates the credentials for the given section",
    CmdOptionValueReq::required, "section_name", [this](const string& value) {
    std::string prompt = mysqlrouter::string_format("Enter password for config section '%s'", value.c_str());
    std::string pass = mysqlrouter::prompt_password(prompt);
    PasswordVault pv;
    pv.update_password(value, pass);
    pv.store_passwords();
    std::cout << "The password was stored in the vault successfully." << std::endl;
    throw silent_exception();
  });

  arg_handler_.add_option(CmdOption::OptionNames({ "--remove-credentials-section" }), "Removes the credentials for the given section",
    CmdOptionValueReq::required, "section_name", [this](const string& value) {
    PasswordVault pv;
    pv.remove_password(value);
    pv.store_passwords();
    std::cout << "The password was removed successfully." << std::endl;
    throw silent_exception();
  });

  arg_handler_.add_option(CmdOption::OptionNames({ "--clear-all-credentials" }), "Clear the vault, removing all the credentials stored on it",
    CmdOptionValueReq::none, "", [this](const string&) {
    PasswordVault pv;
    pv.clear_passwords();
    std::cout << "Removed successfully all passwords from the vault." << std::endl;
    throw silent_exception();
  });
#endif

}

void MySQLRouter::bootstrap(const std::string &server_url) {
  mysqlrouter::ConfigGenerator config_gen{
#ifndef _WIN32
      sys_user_operations_
#endif
  };
  config_gen.init(server_url, bootstrap_options_); // throws std::runtime_error
  config_gen.warn_on_no_ssl(bootstrap_options_);   // throws std::runtime_error

#ifdef _WIN32
  // Cannot run boostrap mode as windows service since it requires console interaction.
  if (mysqlrouter::is_running_as_service())
  {
    std::string msg = "Cannot run router in boostrap mode as Windows service.";
    mysqlrouter::write_windows_event_log(msg);
    throw std::runtime_error(msg);
  }
#endif

  auto defualt_paths = get_default_paths();

  if (bootstrap_directory_.empty()) {
    std::string config_file_path =
        substitute_variable(MYSQL_ROUTER_CONFIG_FOLDER"/mysqlrouter.conf",
                              "{origin}", origin_.str());
    std::string master_key_path =
        substitute_variable(MYSQL_ROUTER_CONFIG_FOLDER"/mysqlrouter.key",
                              "{origin}", origin_.str());
    std::string default_keyring_file;
    default_keyring_file = substitute_variable(MYSQL_ROUTER_DATA_FOLDER,
                                                 "{origin}", origin_.str());
    mysql_harness::Path keyring_dir(default_keyring_file);
    if (!keyring_dir.exists()) {
      if (mysqlrouter::mkdir(default_keyring_file, mysqlrouter::kStrictDirectoryPerm) < 0) {
        std::cerr << "Cannot create directory " << default_keyring_file << ": " << get_strerror(errno) << "\n";
        throw std::runtime_error("Could not create keyring directory");
      } else {
        // sets the directory owner for the --user if provided
        config_gen.set_file_owner(bootstrap_options_, default_keyring_file);
        default_keyring_file = keyring_dir.real_path().str();
      }
    }
    default_keyring_file.append("/").append(kDefaultKeyringFileName);
    config_gen.bootstrap_system_deployment(config_file_path,
        bootstrap_options_, defualt_paths, default_keyring_file, master_key_path);
  } else {
    config_gen.bootstrap_directory_deployment(bootstrap_directory_,
        bootstrap_options_, defualt_paths, kDefaultKeyringFileName, "mysqlrouter.key");
  }
}


void MySQLRouter::show_help() noexcept {
  FILE *fp;
  std::cout << get_version_line() << std::endl;
  std::cout << WELCOME << std::endl;

  for (auto line: wrap_string("Configuration read from the following files in the given order"
                                  " (enclosed in parentheses means not available for reading):", kHelpScreenWidth,
                              0)) {
    std::cout << line << std::endl;
  }

  for (auto file : default_config_files_) {

    if ((fp = std::fopen(file.c_str(), "r")) == nullptr) {
      std::cout << "  (" << file << ")" << std::endl;
    } else {
      std::fclose(fp);
      std::cout << "  " << file << std::endl;
    }
  }
  const std::map<std::string, std::string> paths = get_default_paths();
  std::cout << "Plugins Path:" << std::endl <<
      "  " << paths.at("plugin_folder") << std::endl;
  std::cout << "Default Log Directory:" << std::endl <<
      "  " << paths.at("logging_folder") << std::endl;
  std::cout << "Default Persistent Data Directory:" << std::endl <<
      "  " << paths.at("data_folder") << std::endl;
  std::cout << "Default Runtime State Directory:" << std::endl <<
      "  " << paths.at("runtime_folder") << std::endl;
  std::cout << std::endl;

  show_usage();
}

void MySQLRouter::show_usage(bool include_options) noexcept {
  for (auto line: arg_handler_.usage_lines("Usage: mysqlrouter", "", kHelpScreenWidth)) {
    std::cout << line << std::endl;
  }

  if (!include_options) {
    return;
  }

  std::cout << "\nOptions:" << std::endl;
  for (auto line: arg_handler_.option_descriptions(kHelpScreenWidth, kHelpScreenIndent)) {
    std::cout << line << std::endl;
  }

#ifdef _WIN32
  std::cout << "\nExamples:\n"
            << "  Bootstrap for use with InnoDB cluster into system-wide installation\n"
            << "    mysqlrouter --bootstrap root@clusterinstance01\n"
            << "  Start router\n"
            << "    mysqlrouter\n"
            << "\n"
            << "  Bootstrap for use with InnoDb cluster in a self-contained directory\n"
            << "    mysqlrouter --bootstrap root@clusterinstance01 -d myrouter\n"
            << "  Start router\n"
            << "    myrouter\\start.ps1\n";
#else
  std::cout << "\nExamples:\n"
            << "  Bootstrap for use with InnoDB cluster into system-wide installation\n"
            << "    sudo mysqlrouter --bootstrap root@clusterinstance01 --user=mysqlrouter\n"
            << "  Start router\n"
            << "    sudo mysqlrouter --user=mysqlrouter&\n"
            << "\n"
            << "  Bootstrap for use with InnoDb cluster in a self-contained directory\n"
            << "    mysqlrouter --bootstrap root@clusterinstance01 -d myrouter\n"
            << "  Start router\n"
            << "    myrouter/start.sh\n";
#endif
  std::cout << "\n";
}

void MySQLRouter::show_usage() noexcept {
  show_usage(true);
}
