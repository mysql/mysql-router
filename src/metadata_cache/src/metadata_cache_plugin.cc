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

#include "metadata_cache.h"
#include "plugin_config.h"

#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"
#include "logger.h"
#include "config_parser.h"

using metadata_cache::LookupResult;
using mysqlrouter::TCPAddress;
using std::string;

const mysql_harness::AppInfo *g_app_info;
static const string kSectionName = "metadata_cache";

static const char *kRoutingRequires[] = {
    "logger",
};

// We can modify the AppInfo object; we need to store password separately
using PasswordKey = std::pair<string, string>;
string metadata_cache_password;

/**
 * Prompt for the password that will be used for accessing the metadata
 * in the metadata servers.
 *
 * @param prompt The password prompt.
 *
 * @return a string representing the password.
 */
#ifndef _WIN32
const string prompt_password(const string &prompt) {
  struct termios console;
  tcgetattr(STDIN_FILENO, &console);

  std::cout << prompt << ": ";

  // prevent showing input
  console.c_lflag &= ~(uint)ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &console);

  string result;
  std::getline(std::cin, result);

  // reset
  console.c_lflag |= ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &console);

  std::cout << std::endl;
  return result;
}
#else
const string prompt_password(const string &prompt) {

  std::cout << prompt << ": ";

  // prevent showing input
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode;
  GetConsoleMode(hStdin, &mode);
  mode &= ~ENABLE_ECHO_INPUT;
  SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));

  string result;
  std::getline(std::cin, result);

  // reset
  SetConsoleMode(hStdin, mode);

  std::cout << std::endl;
  return result;
}
#endif

/**
 * Load the metadata cache configuration from the router config file.
 *
 * @param info the object encapuslating the router configuration.
 *
 * @return 0 if the configuration was read successfully.
 */
static int init(const mysql_harness::AppInfo *info) {
  g_app_info = info;
  const mysql_harness::ConfigSection *section;
  // If a valid configuration object was found.
  if (info && info->config) {
    // if a valid metadata_cache section was found in the router
    // configuration.
    if (!(info->config->get(kSectionName).empty())) {
      section = (info->config->get(kSectionName)).front();
    } else {
      throw std::invalid_argument("[metadata_cache] section is empty");
    }

    // Create a configuration object encapsulating the metadata cache
    // information.
    MetadataCachePluginConfig config(section); // raises on errors
    if (section->has("password")) {
      throw std::invalid_argument(
        "'password' option is not allowed in the configuration file. "
        "Router will prompt for password instead.");
    }
    // we need to prompt for the password
    auto prompt = mysqlrouter::string_format("Password for [%s%s%s], user %s",
                                             section->name.c_str(),
                                             section->key.empty() ? "" : ":",
                                             section->key.c_str(),
                                             config.user.c_str());
    metadata_cache_password = prompt_password(prompt);
  }
  return 0;
}

/**
 * Initialize the metadata cache for fetching the information from the
 * metadata servers.
 *
 * @param section An object encapsulating the metadata cache information.
 */
static void start(const mysql_harness::ConfigSection *section) {
 try {
    MetadataCachePluginConfig config(section);
    unsigned int ttl{config.ttl};
    string metadata_replicaset{config.metadata_replicaset};

    // Initialize the defaults.
    ttl = ttl == 0 ? metadata_cache::kDefaultMetadataTTL : ttl;
    metadata_replicaset = metadata_replicaset.empty()?
      metadata_cache::kDefaultMetadataReplicaset : metadata_replicaset;

    log_info("Starting Metadata Cache");

    // Initialize the metadata cache.
    metadata_cache::cache_init(config.bootstrap_addresses, config.user,
                               metadata_cache_password, ttl,
                               metadata_replicaset);
  } catch (const std::runtime_error &exc) {
    // We continue and retry
    log_error(exc.what());
  } catch (const std::invalid_argument &exc) {
    log_error(exc.what());
    return;
  }
}

extern "C" {

mysql_harness::Plugin METADATA_API harness_plugin_metadata_cache = {
    mysql_harness::PLUGIN_ABI_VERSION,
    mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "Metadata Cache, managing information fetched from the Metadata Server",
    VERSION_NUMBER(0, 0, 1),
    sizeof(kRoutingRequires) / sizeof(*kRoutingRequires), kRoutingRequires, // Requires
    0, NULL,                                      // Conflicts
    init,
    NULL,
    start,                                       // start
    NULL                                         // stop
};

}
