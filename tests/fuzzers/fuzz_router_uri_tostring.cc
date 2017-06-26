/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <exception>
#include <sstream>

#include "mysqlrouter/uri.h"

using mysqlrouter::URIError;
using mysqlrouter::URIParser;
using mysqlrouter::URI;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  URI uri_a;

  /*
   * assume that every valid URI that we parse can be:
   *
   * 1) turned into a string
   * 2) parsed again without exceptions
   * 3) turned into a string again that matches the string of 1)
   */

  try {
    // turn the random input into something we can turn into a string
    uri_a = URIParser::parse(std::string(Data, Data + Size));
  } catch(URIError &e) {
    // ignore parse errors
    return 0;
  }

  // shouldn't throw
  std::ostringstream uri_a_ss;

  uri_a_ss << uri_a;

  // parse what we generated. It shouldn't throw.
  URI uri_b(URIParser::parse(uri_a_ss.str()));

  if (uri_a != uri_b) {
    std::string err("URI fields differ: ");

#define APPEND_IF_NE(a, b, fld) \
    if (a.fld != b.fld) err += std::string(#fld ": ") + a.fld + " != " + b.fld + ", ";

    APPEND_IF_NE(uri_a, uri_b, scheme);
    APPEND_IF_NE(uri_a, uri_b, host);
    if (uri_a.port != uri_b.port) err += std::string("port: ") + std::to_string(uri_a.port) + " != " + std::to_string(uri_b.port) + ", ";
    APPEND_IF_NE(uri_a, uri_b, username);
    APPEND_IF_NE(uri_a, uri_b, password);

    // TODO: map/array printer
    if (uri_a.path != uri_b.path) err += std::string("path: <skipped>") + ", ";
    if (uri_a.query != uri_b.query) err += std::string("query: <skipped>") + ", ";
    APPEND_IF_NE(uri_a, uri_b, fragment);
#undef APPEND_IF_NE

    throw std::runtime_error(err);
  }
  // the components should be equal too
  // and it should be equal to what we fed to the parser.
  std::ostringstream uri_b_ss;

  uri_b_ss << uri_b;

  if (uri_a_ss.str() != uri_b_ss.str()) {
    throw std::runtime_error("URIs differ: " + uri_a_ss.str() + " != " + uri_b_ss.str());
  }

  return 0;
}
