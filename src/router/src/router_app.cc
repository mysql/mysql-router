/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "mysqlrouter/utils.h"
#include "router_app.h"
#include "utils.h"
#include "config_generator.h"

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
  return r;
}


MySQLRouter::MySQLRouter(const mysql_harness::Path& origin, const vector<string>& arguments)
    : version_(MYSQL_ROUTER_VERSION_MAJOR, MYSQL_ROUTER_VERSION_MINOR, MYSQL_ROUTER_VERSION_PATCH),
      arg_handler_(), loader_(), can_start_(false),
      showing_info_(false),
      origin_(origin)
{
  init(arguments);
}

MySQLRouter::MySQLRouter(const int argc, char **argv)
    : MySQLRouter(mysql_harness::Path(find_full_path(argv[0])).dirname(),
                  vector<string>({argv + 1, argv + argc}))
{
}

void MySQLRouter::init(const vector<string>& arguments) {
  set_default_config_files(CONFIG_FILES);
  prepare_command_options();
  try {
    arg_handler_.process(arguments);
  } catch (const std::invalid_argument &exc) {
    throw std::runtime_error(exc.what());
  }

  if (!bootstrap_uri_.empty()) {
    bootstrap(bootstrap_uri_);
    return;
  }
  if (showing_info_) {
    return;
  }

  available_config_files_ = check_config_files();
  can_start_ = true;
}

void MySQLRouter::start() {
  if (showing_info_ || !bootstrap_uri_.empty()) {
    // when we are showing info like --help or --version, we do not throw
    return;
  }
  if (!can_start_) {
    throw std::runtime_error("Can not start");
  }
  string err_msg = "Configuration error: %s.";

  std::map<std::string, std::string> params = {
      {"program", "mysqlrouter"},
      {"origin", origin_.str()},
      {"logging_folder", string(MYSQL_ROUTER_LOGGING_FOLDER)},
      {"plugin_folder", string(MYSQL_ROUTER_PLUGIN_FOLDER)},
      {"runtime_folder", string(MYSQL_ROUTER_RUNTIME_FOLDER)},
      {"config_folder", string(MYSQL_ROUTER_CONFIG_FOLDER)}
  };

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

  try {
    auto log_file = loader_->get_log_file();
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

  for (vector<string> *vec: config_file_containers) {
    for (auto &file: *vec) {
      auto pos = std::find(result.begin(), result.end(), file);
      if (pos != result.end()) {
        throw std::runtime_error(string_format("Duplicate configuration file: %s.", file.c_str()));
      }
      std::ifstream file_check;
      file_check.open(file);
      if (file_check.is_open()) {
        result.push_back(file);
        if (vec != &extra_config_files_) {
          nr_of_none_extra++;
        }
      }
    }
  }

  // Can not have extra configuration files when we do not have other configuration files
  if (!extra_config_files_.empty() && nr_of_none_extra == 0) {
    throw std::runtime_error("Extra configuration files only work when other configuration files are available.");
  }

  if (result.empty()) {
    throw std::runtime_error("No valid configuration file available. See --help for more information.");
  }

  return result;
}

void MySQLRouter::prepare_command_options() noexcept {
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
        this->bootstrap_uri_ = server_url;
      });

  arg_handler_.add_option(OptionNames({"-d", "--directory"}),
                          "Creates a self-contained directory for a new instance of the Router. Requires --bootstrap",
                          CmdOptionValueReq::required, "path",
                          [this](const string &path) {
        this->bootstrap_directory_ = path;
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option -d/--directory can only be used together with -B/--bootstrap");
        }
      });

#ifndef _WIN32
  arg_handler_.add_option(OptionNames({"--conf-use-sockets"}),
                          "Whether to use Unix domain sockets instead of TCP. Requires --bootstrap",
                          CmdOptionValueReq::none, "",
                          [this](const string &) {
        this->bootstrap_options_["use-sockets"] = "1";
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option --conf-use-sockets can only be used together with -B/--bootstrap");
        }
      });
#endif
  arg_handler_.add_option(OptionNames({"--conf-base-port"}),
                          "Base port to use for listening router ports. Requires --bootstrap",
                          CmdOptionValueReq::required, "port",
                          [this](const string &port) {
        this->bootstrap_options_["base-port"] = port;
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option --conf-base-port can only be used together with -B/--bootstrap");
        }
      });

  arg_handler_.add_option(OptionNames({"--name"}),
                          "Gives a symbolic name for the router instance. Requires --bootstrap",
                          CmdOptionValueReq::required, "name",
                          [this](const string &name) {
        this->bootstrap_options_["name"] = name;
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option --name can only be used together with -B/--bootstrap");
        }
      });

  arg_handler_.add_option(OptionNames({"--force"}),
                          "Force reconfiguration of a possibly existing instance of the router. Requires --bootstrap",
                          CmdOptionValueReq::none, "",
                          [this](const string &) {
        this->bootstrap_options_["force"] = "1";
        if (this->bootstrap_uri_.empty()) {
          throw std::runtime_error("Option --force can only be used together with -B/--bootstrap");
        }
      });

  arg_handler_.add_option(OptionNames({"-c", "--config"}),
                          "Only read configuration from given file.",
                          CmdOptionValueReq::required, "path", [this](const string &value) throw(std::runtime_error) {

        if (!config_files_.empty()) {
          throw std::runtime_error("Option -c/--config can only be used once; use -a/--extra-config instead.");
        }

        // When --config is used, no defaults shall be read
        default_config_files_.clear();

        std::string abspath = mysql_harness::Path(value).real_path().str();
        if (!abspath.empty()) {
          config_files_.push_back(abspath);
        } else {
          throw std::runtime_error(string_format("Failed reading configuration file: %s", value.c_str()));
        }
      });

  arg_handler_.add_option(CmdOption::OptionNames({"-a", "--extra-config"}),
                          "Read this file after configuration files are read from either "
                              "default locations or from files specified by the --config option.",
                          CmdOptionValueReq::required, "path", [this](const string &value) throw(std::runtime_error) {
        std::string abspath = mysql_harness::Path(value).real_path().str();
        if (!abspath.empty()) {
          extra_config_files_.push_back(abspath);
        } else {
          throw std::runtime_error(string_format("Failed reading configuration file: %s", value.c_str()));
        }
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
  ConfigGenerator config_gen;
  config_gen.init(server_url);

  if (bootstrap_directory_.empty()) {
    std::string config_file_path =
        substitute_variable(MYSQL_ROUTER_CONFIG_FOLDER"/mysqlrouter.conf",
                            "{origin}", origin_.str());

    config_gen.bootstrap_system_deployment(config_file_path,
        bootstrap_options_);
  } else {
    config_gen.bootstrap_directory_deployment(bootstrap_directory_,
        bootstrap_options_);
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

  std::cout << "\n";
}

void MySQLRouter::show_usage() noexcept {
  show_usage(true);
}
