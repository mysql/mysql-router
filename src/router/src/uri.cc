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

#include "mysqlrouter/uri.h"
#include "utils.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <memory>

#include "mysqlrouter/utils.h"

namespace mysqlrouter {

const string kValidSchemeChars = "abcdefghijklmnopqrstuvwxyz0123456789+-."; // scheme is lowered first
const string kValidPortChars = "0123456789";

static string parse_fragment(const string uri) {
  size_t pos;

  // Basic check
  pos = uri.find("://");
  if (pos == string::npos) {
    throw URIError("invalid URI");
  }

  pos = uri.find('#', pos + 3);
  if (pos == string::npos) {
    // No fragment
    return "";
  }

  return uri.substr(pos + 1);
}


static string parse_scheme(const string uri) {
  string tmp_scheme;
  auto pos = uri.find(':');
  if (pos == std::string::npos) {
    throw URIError("no colon separator found while parsing scheme");
  }
  tmp_scheme = uri.substr(0, pos);

  // scheme is always ASCII
  std::transform(tmp_scheme.begin(), tmp_scheme.end(), tmp_scheme.begin(), ::tolower);

  if (tmp_scheme.find_first_not_of(kValidSchemeChars, 0) != std::string::npos) {
    throw URIError("bad URI or scheme contains invalid character(s)");
  }

  return tmp_scheme;
}

static URIAuthority parse_authority(const string uri) {
  string authority;
  string user_info;
  string host_port;
  size_t pos_start;
  size_t pos_end;
  size_t pos;
  size_t pos_at;
  string tmp_port_str;
  string tmp_host;
  uint64_t tmp_port = 0;
  string tmp_username;
  string tmp_password;

  pos_start = uri.find("//");
  if (pos_start == string::npos) {
    throw URIError("start of authority not found in URI (no //)");
  }
  // URI without path (no foward slash after authority)
  pos_end = uri.find('/', pos_start + 2);
  authority = uri.substr(pos_start + 2, pos_end - (pos_start + 2));

  if (authority.empty()) {
    return URIAuthority{};
  }

  // Get user information
  pos_at = authority.find('@');
  if (pos_at != string::npos) {
    user_info = authority.substr(0, pos_at);
    pos = user_info.find(':');
    if (pos != string::npos) {
      tmp_username = user_info.substr(0, pos);
      tmp_password = user_info.substr(pos + 1, user_info.size() - (pos + 1));
    } else {
      // No password
      tmp_username = user_info;
      tmp_password = "";
    }
  }
  if (tmp_username.empty() && !tmp_password.empty()) {
    throw URIError("password but no username given");
  }

  host_port = authority.substr(pos_at + 1);

  // Get host and port
  pos = host_port.find(':');
  if (pos != string::npos) {
    try {
      tmp_port = get_tcp_port(host_port.substr(pos + 1, authority.size() - (pos + 1)));
    } catch (const std::runtime_error &exc) {
      throw URIError("invalid port: " + string(exc.what()));
    }
    tmp_host = host_port.substr(0, pos);
  } else {
    tmp_host = host_port;
  }

  if (tmp_host.empty()) {
    throw URIError("invalid host");
  }

  return URIAuthority(tmp_host, tmp_port, tmp_username, tmp_password);
}

static URIPath parse_path(const string uri) {
  size_t pos;
  size_t pos_end;
  string tmp_path;

  // Basic check
  pos = uri.find("://");
  if (pos == string::npos) {
    throw URIError("invalid URI");
  }

  pos = uri.find('/', pos + 3);
  if (pos == string::npos) {
    // No path, we clear what we had
    return {};
  }

  pos_end = uri.find('?', pos + 1);
  // We skip the first forward slash
  if (pos_end == string::npos) {
    tmp_path = uri.substr(pos + 1);
  } else {
    tmp_path = uri.substr(pos + 1, pos_end - (pos + 1));
  }

  return split_string(tmp_path, '/', false);
}

static URIQuery parse_query(const string &uri, const char delimiter) {
  size_t pos;
  size_t pos_end;
  string tmp_query;
  std::vector<string> key_value;
  URIQuery result;

  // Basic check
  pos = uri.find("://");
  if (pos == string::npos) {
    throw URIError("invalid URI");
  }

  pos = uri.find('?', pos + 3);
  if (pos == string::npos) {
    // No query
    return URIQuery();
  }

  pos_end = uri.find('#', pos + 1);
  if (pos_end == string::npos) {
    tmp_query = uri.substr(pos + 1);
  } else {
    tmp_query = uri.substr(pos + 1, pos_end - (pos + 1));
  }

  for (auto& part: split_string(tmp_query, delimiter, false)) {
    key_value = split_string(part, '=');
    if (!key_value.at(0).empty()) {
      result[key_value.at(0)] = key_value.at(1);
    }
  }

  return result;
}

static URIQuery parse_query(const string &uri) {
  return parse_query(uri, URI::query_delimiter);
}

void URI::init_from_uri(const string uri) {
  if (uri.empty()) {
    return;
  }
  string tmp_scheme;
  URIAuthority authority;
  URIPath tmp_path;
  URIQuery tmp_query;
  string tmp_fragment;

  try {
    tmp_scheme = parse_scheme(uri);
    authority = parse_authority(uri);
    tmp_path = parse_path(uri);
    tmp_query = parse_query(uri);
    tmp_fragment = parse_fragment(uri);
  } catch (const URIError &exc) {
    throw URIError(string_format("invalid url: %s", exc.what()));
  }

  scheme = tmp_scheme;
  host = std::get<0>(authority);
  port = std::get<1>(authority);
  username = std::get<2>(authority);
  password = std::get<3>(authority);
  path = tmp_path;
  query = tmp_query;
  fragment = tmp_fragment;
}

#ifdef ENABLE_TESTS
string t_parse_scheme(const string &uri) { return parse_scheme(uri); };
URIAuthority t_parse_authority(const string &uri) { return parse_authority(uri);};
URIPath t_parse_path(const string &uri) { return parse_path(uri);};
URIQuery t_parse_query(const string &uri, const char delimiter) {return parse_query(uri, delimiter);};
URIQuery t_parse_query(const string &uri) {return parse_query(uri);};
string t_parse_fragment(const string &uri) { return parse_fragment(uri);};
#endif

} // namespace mysqlrouter
