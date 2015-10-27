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

#include "fabric_cache.h"
#include "plugin_config.h"

#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"
#include "logger.h"
#include "config_parser.h"

using fabric_cache::LookupResult;
using mysqlrouter::TCPAddress;
using std::string;


const AppInfo *g_app_info;
static const string kSectionName = "fabric_cache";

static const char *kRoutingRequires[] = {
    "logger",
};

// We can modify the AppInfo object; we need to store password separately
using PasswordKey = std::pair<string, string>;
std::map<PasswordKey, string> fabric_cache_passwords;

/** @brief Makes key for cache password map
 *
 * @param addr address of Fabric as TCPAddress object
 * @param user user for Fabric authentication
 * @return pwd_key
 */
PasswordKey make_cache_password(const TCPAddress &addr, const string &user) {
  return std::make_pair(addr.str(), user);
}

/** @brief Returns whether we have already password
 *
 * @param std::pair holding address and username
 * @param
 */
static bool have_cache_password(const PasswordKey &key) {
  return fabric_cache_passwords.find(key) != fabric_cache_passwords.end();
}

const string prompt_password(const string &prompt) {
  struct termios console;
  tcgetattr(STDIN_FILENO, &console);

  std::cout << prompt << ": ";

  // prevent showing input
  console.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &console);

  string result;
  std::cin >> result;

  // reset
  console.c_lflag |= ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &console);

  std::cout << std::endl;
  return result;
}

static int init(const AppInfo *info) {
  g_app_info = info;

  if (info && info->config) {

    if (info->config->get(kSectionName).size() > 1) {
      throw std::invalid_argument("Router supports only 1 fabric_cache section.");
    }

    for (auto &section: info->config->get(kSectionName)) {
      FabricCachePluginConfig config(section); // raises on errors
      fabric_cache::g_fabric_cache_config_sections.push_back(section->key);

      if (section->has("password")) {
        throw std::invalid_argument(
            "'password' option is not allowed in the configuration file. "
                "Router will prompt for password instead.");
      }

      auto password_key = make_cache_password(config.address, section->get("user"));
      if (have_cache_password(password_key)) {
        // we got the password already for this address and user
        continue;
      }
      // we need to prompt for the password
      auto prompt = mysqlrouter::string_format("Password for [%s%s%s], user %s",
                                               section->name.c_str(),
                                               section->key.empty() ? "" : ":",
                                               section->key.c_str(), config.user.c_str());
      auto password = prompt_password(prompt);
      fabric_cache_passwords.emplace(std::make_pair(password_key, password));
    }
  }

  return 0;
}

static void start(const ConfigSection *section) {
  string name_tag = string();

  if (!section->key.empty()) {
    name_tag = "'" + section->key + "' ";
  }

  try {
    FabricCachePluginConfig config(section);
    int port{config.address.port};

    port = port == 0 ? fabric_cache::kDefaultFabricPort : port;

    log_info("Starting Fabric Cache %susing MySQL Fabric running on %s",
             name_tag.c_str(), config.address.str().c_str());
    auto password_key = make_cache_password(config.address, section->get("user"));
    auto found = fabric_cache_passwords.find(password_key);
    string password {};  // empty password for when authentication is disabled on Fabric
    if (found != fabric_cache_passwords.end()) {
      password = found->second;
    }
    fabric_cache::cache_init(section->key, config.address.addr, port, config.user, password);

  } catch (const fabric_cache::base_error &exc) {
    // We continue and retry
    log_error(exc.what());
  } catch (const std::invalid_argument &exc) {
    log_error(exc.what());
    return;
  }
}

Plugin harness_plugin_fabric_cache = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "Fabric Cache, managing information fetched from MySQL Fabric",
    VERSION_NUMBER(0, 0, 1),
    sizeof(kRoutingRequires) / sizeof(*kRoutingRequires), kRoutingRequires, // Requires
    0, NULL,                                      // Conflicts
    init,
    NULL,
    start                                        // start
};
