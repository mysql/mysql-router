/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <ctime>     // mktime

#include <regex>     // date parser
#include <iostream>  // cerr

#include <event2/util.h>

#include "mysqlrouter/http_common.h"

static int ts_to_rfc1123(time_t ts, char *date_buf, size_t date_buf_len) {
  struct tm t_m {};

  gmtime_r(&ts, &t_m);

  const char *DAYS[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

  const char *MONTH[] = {
    "Jan", "Feb", "Mar", "Apr", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  return evutil_snprintf(date_buf, date_buf_len, "%s, %02d %s %4d %02d:%02d:%02d GMT",
    DAYS[t_m.tm_wday], t_m.tm_mday, MONTH[t_m.tm_mon],
    1900 + t_m.tm_year, t_m.tm_hour, t_m.tm_min, t_m.tm_sec);
}

static time_t ts_from_http_date(const char *date_buf) {
  std::regex http_date_re { "^(Sun|Mon|Tue|Wed|Thu|Fri|Sat|Sun), ([0-9]{2}) (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) ([0-9]{4}) ([0-9]{2}):([0-9]{2}):([0-9]{2}) GMT" };

  std::cmatch fields;

  if (std::regex_search(date_buf, fields, http_date_re)) {
    struct tm t_m {};

    t_m.tm_mday = std::strtol(fields[2].str().c_str(), nullptr, 10);
    t_m.tm_mon = std::map<std::string, decltype(t_m.tm_mon)> {
      { "Jan", 0 },
      { "Feb", 1 },
      { "Mar", 2 },
      { "Apr", 3 },
      { "May", 4 },
      { "Jun", 5 },
      { "Jul", 6 },
      { "Aug", 7 },
      { "Sep", 8 },
      { "Oct", 9 },
      { "Nov", 10 },
      { "Dec", 11 },
    }[fields[3].str()];
    t_m.tm_year = std::strtol(fields[4].str().c_str(), nullptr, 10) - 1900;
    t_m.tm_hour = std::strtol(fields[5].str().c_str(), nullptr, 10);
    t_m.tm_min = std::strtol(fields[6].str().c_str(), nullptr, 10);
    t_m.tm_sec = std::strtol(fields[7].str().c_str(), nullptr, 10);

    // mktime returns localhost, but we need GMT
    return mktime(&t_m) - timezone;
  } else {
    std::cerr << "regex failed for " << date_buf << std::endl;
  }
  return 0;
}


bool is_modified_since(const HttpRequest &req, time_t last_modified) {
  auto req_hdrs = req.get_input_headers();

  auto *if_mod_since = req_hdrs.get("If-Modified-Since");
  if (if_mod_since != nullptr) {
    time_t if_mod_since_ts = ts_from_http_date(if_mod_since);

    if (!(last_modified > if_mod_since_ts)) {
      return false;
    }
  }
  return true;
}

void add_last_modified(HttpRequest &req, time_t last_modified) {
  auto out_hdrs = req.get_output_headers();
  char date_buf[50];

  if (sizeof(date_buf) - ts_to_rfc1123(last_modified, date_buf, sizeof(date_buf)) > 0) {
    out_hdrs.add("Last-Modified", date_buf);
  }
}


