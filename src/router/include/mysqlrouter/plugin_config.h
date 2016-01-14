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

#ifndef MYSQLROUTER_PLUGIN_CONFIG_INCLUDED
#define MYSQLROUTER_PLUGIN_CONFIG_INCLUDED

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"

#include <cerrno>
#include <cstdlib>
#include <map>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include <iostream>

#include "config_parser.h"
#include "logger.h"

using std::string;

namespace mysqlrouter {

/** @class BasePluginConfig
 * @brief Retrieve and manage plugin configuration
 *
 * BasePluginConfig is an abstract class which can be used to by plugins
 * to derive their own class retrieving configuration from, for example,
 * Harness `ConfigSection instances`.
 */
class BasePluginConfig {
public:
  using defaults_map = std::map<string, string>;

  /** @brief Constructor
   *
   * @param section from configuration file provided as ConfigSection
   */
  BasePluginConfig() { }

  /** @brief Gets value of given option as string
   *
   * @param section Instance of ConfigSection
   * @return Option value as std::string
   */
  string get_option_string(const ConfigSection *section, const string &option);

  /** @brief Name of the section */
  string section_name;

protected:
  /** @brief Constructor for derived classes */
  BasePluginConfig(const ConfigSection *section) : section_name(get_section_name(section)) {}

  /** @brief Generate the name for this configuration
   *
   * @param section Instance of ConfigSection
   * @return the name for this configuration
   */
  virtual string get_section_name(const ConfigSection *) const noexcept;

  /** @brief Gets the default for the given option
   *
   * Gets the default value of the given option. If no default option
   * is available, an empty string is returned.
   *
   * @param string option
   * @return default value for given option as std::string
   */
  virtual string get_default(const string &option) = 0;

  /** @brief Returns whether the given option is required
   *
   * @return bool
   */
  virtual bool is_required(const string &option) = 0;

  /** @brief Gets message prefix for option and section
   *
   * Gets the message prefix of option and section. The option
   * name will be mentioned as well as the section from the configuration.
   *
   * For example, option wait_timeout in section [routing:homepage] will
   * return a prefix (without quotes):
   *   "option wait_timeout in [routing:homepage]"
   *
   * This is useful when reporting errors.
   *
   * @param option Name of the option
   * @return Prefix as std::string
   */
  virtual string get_log_prefix(const string &option) const noexcept;

  /** @brief Gets an unsigned integer using the given option
   *
   * Gets an unsigned integer using the given option. The type can be
   * any unsigned integer type such as uint16_t.
   *
   * The min_value argument can be used to set a minimum value for
   * the option. For example, when 0 (zero) is not allowed, min_value
   * can be set to 0. The maximum value is whatever the maximum of the
   * use type is.
   *
   * Throws std::invalid_argument on errors.
   *
   * @param section Instance of ConfigSection
   * @param option Option name in section
   * @param min_value Minimum value
   * @return mysqlrouter::TCPAddress
   */
  template<typename T>
  T get_uint_option(const ConfigSection *section, const string &option,
                    T min_value = 0, T max_value = std::numeric_limits<T>::max()) {
    string value = get_option_string(section, option);

    long result;
    char *rest;
    errno = 0;
    result = std::strtol(value.c_str(), &rest, 0);

    if (errno > 0 || *rest != '\0' || result > max_value || result < min_value ||
        (max_value > 0 && result > max_value)) {
      std::ostringstream os;
      os << get_log_prefix(option) << " needs value between " << min_value << " and "
         << to_string(max_value) << " inclusive";
      if (!value.empty()) {
        os << ", was '" << value << "'";
      }
      throw std::invalid_argument(os.str());
    }
    return static_cast<T>(result);
  }

  /** @brief Gets a TCP address using the given option
   *
   * Gets a TCP address using the given option. The option value is
   * split in 2 giving the IP (or address) and the TCP Port. When
   * require_port is true, a valid port number will be required.
   *
   * Throws std::invalid_argument on errors.
   *
   * @param section Instance of ConfigSection
   * @param option Option name in section
   * @param require_port Whether a TCP port is required
   * @return mysqlrouter::TCPAddress
   */
  TCPAddress get_option_tcp_address(const ConfigSection *section, const string &option,
                                    bool require_port = false, int default_port = -1);

  int get_option_tcp_port(const ConfigSection *section, const string &option);

};

} // namespace mysqlrouter

#endif // MYSQLROUTER_PLUGIN_CONFIG_INCLUDED
