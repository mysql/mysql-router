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

#ifndef URI_ROUTING_INCLUDED
#define URI_ROUTING_INCLUDED

#include "config.h"

#include <exception>
#include <map>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>

using std::string;

namespace mysqlrouter {

using URIAuthority = std::tuple<string, uint16_t, string, string>; // host, port, username, password
using URIPath = std::vector<string>;
using URIQuery = std::map<string, string>;

/** @class URIError
 * @brief Exception when URI was not valid
 *
 */
class URIError : public std::runtime_error {
public:
  explicit URIError(const string &what_arg) : std::runtime_error(what_arg) { }
};

/** @class URI
 * @brief Parse and create URIs according to RFC3986
 *
 * This class will parse and make the elements of the URI
 * available as members.
 *
 * Links:
 * * (RFC 3986)[https://tools.ietf.org/html/rfc3986)
 *
 */
class URI {
public:
  /** @brief Delimiter used in the Query part */
  static const char query_delimiter = '&';

  /** @brief Default constructor
   *
   * @param uri URI to be used to read parts
   */
  URI(const string &uri) : scheme(), host(), port(0), username(), password(), path(), query(),
                           fragment(), uri_(uri) {
    if (!uri.empty()) {
      init_from_uri(uri);
    }
  };

  /** @brief overload */
  URI() : URI("") { };

  /** @brief Sets URI using the given URI string
   *
   * @param uri URI as string
   */
  void set_uri(const string &uri) {
    init_from_uri(uri);
  }

  /** @brief Scheme of the URI */
  string scheme;
  /** @brief Host part found in the Authority */
  string host;
  /** @brief Port found in the Authority */
  uint16_t port;  // 0 means use default (no dynamically allocation needed here)
  /** @brief Username part found in the Authority */
  string username;
  /** @brief Password part found in the Authority */
  string password;
  /** @brief Path part of the URI */
  URIPath path;
  /** @brief Query part of the URI */
  URIQuery query;
  /** @brief Fragment part of the URI */
  string fragment;

private:
  /** @brief Sets information using the given URI
   *
   * Takes a and parsers out all URI elements.
   *
   * Throws URIError on errors.
   *
   * @param string URI to use
   */
  void init_from_uri(const string uri);

  /** @brief Copy of the original given URI */
  string uri_;

};

#ifdef ENABLE_TESTS
// Testing statics
string t_parse_scheme(const string &uri);
URIAuthority t_parse_authority(const string &uri);
URIPath t_parse_path(const string &uri);
URIQuery t_parse_query(const string &uri, const char delimiter);
URIQuery t_parse_query(const string &uri);
string t_parse_fragment(const string &uri);
#endif

} // namespace mysqlrouter

#endif // URI_ROUTING_INCLUDED
