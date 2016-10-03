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

#include "mysqlrouter/mysql_session.h"
#include "logger.h"
#include <sstream>
#include <mysql.h>

using namespace mysqlrouter;

MySQLSession::MySQLSession() {
  connection_ = new MYSQL();
  connected_ = false;
  mysql_init(connection_);
}

MySQLSession::~MySQLSession() {
  disconnect();
  delete connection_;
}

void MySQLSession::connect(const std::string &host, unsigned int port,
                           const std::string &username,
                           const std::string &password,
                           int connection_timeout) {
  disconnect();
  unsigned int protocol = MYSQL_PROTOCOL_TCP;
  connected_ = false;

  // Following would fail only when invalid values are given. It is not possible
  // for the user to change these values.
  mysql_options(connection_, MYSQL_OPT_CONNECT_TIMEOUT,
                &connection_timeout);
  mysql_options(connection_, MYSQL_OPT_PROTOCOL,
                reinterpret_cast<char *> (&protocol));

  const unsigned long client_flags = (
    CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 |
    CLIENT_MULTI_RESULTS
    );
  if (!mysql_real_connect(connection_, host.c_str(), username.c_str(),
                         password.c_str(), nullptr,
                         port, nullptr,
                         client_flags)) {
    std::stringstream ss;
    ss << "Error connecting to MySQL server at " << host << ":" << port;
    ss << ": " << mysql_error(connection_) << " (" << mysql_errno(connection_) << ")";
    throw Error(ss.str().c_str(), mysql_errno(connection_));
  }
}

void MySQLSession::disconnect() {
  if (connected_) {
    mysql_close(connection_);
    connected_ = false;
  }
}

void MySQLSession::execute(const std::string &query) {
  if (connection_) {
    if (mysql_real_query(connection_, query.data(), query.length()) != 0) {
      std::stringstream ss;
      ss << "Error executing MySQL query";
      ss << ": " << mysql_error(connection_) << " (" << mysql_errno(connection_) << ")";
      throw Error(ss.str().c_str(), mysql_errno(connection_));
    }
    MYSQL_RES *res = mysql_store_result(connection_);
    if (res)
      mysql_free_result(res);
  }
}

/*
  Execute query on the session and iterate the results with the given callback.

  The processor callback is called with a vector of strings, which conain the
  values of each field of a row. It is called once per row.
  If the processor returns false, the result row iteration stops.
 */
void MySQLSession::query(const std::string &query,
                         const RowProcessor &processor) {
  if (connection_) {
    if (mysql_real_query(connection_, query.data(), query.length()) != 0) {
      std::stringstream ss;
      ss << "Error executing MySQL query";
      ss << ": " << mysql_error(connection_) << " (" << mysql_errno(connection_) << ")";
      throw Error(ss.str().c_str(), mysql_errno(connection_));
    }
    MYSQL_RES *res = mysql_use_result(connection_);
    if (res) {
      unsigned int nfields = mysql_num_fields(res);
      std::vector<const char*> outrow;
      outrow.resize(nfields);
      MYSQL_ROW row;
      while ((row = mysql_fetch_row(res))) {
        for (unsigned int i = 0; i < nfields; i++) {
          outrow[i] = row[i];
        }
        try {
          if (!processor(outrow))
            break;
        } catch (...) {
          mysql_free_result(res);
          throw;
        }
      }
      mysql_free_result(res);
    } else {
      std::stringstream ss;
      ss << "Error fetching query results: ";
      ss << mysql_error(connection_) << " (" << mysql_errno(connection_) << ")";
      throw Error(ss.str().c_str(), mysql_errno(connection_));
    }
  }
}

class RealResultRow : public MySQLSession::ResultRow {
public:
  RealResultRow(const MySQLSession::Row &row, MYSQL_RES *res)
  : res_(res) {
    row_ = row;
  }

  virtual ~RealResultRow() {
    mysql_free_result(res_);
  }
private:
  MYSQL_RES *res_;
};

MySQLSession::ResultRow *MySQLSession::query_one(const std::string &query) {
  if (connection_) {
    if (mysql_real_query(connection_, query.data(), query.length()) != 0) {
      std::stringstream ss;
      ss << "Error executing MySQL query";
      ss << ": " << mysql_error(connection_) << " (" << mysql_errno(connection_) << ")";
      throw Error(ss.str().c_str(), mysql_errno(connection_));
    }
    MYSQL_RES *res = mysql_use_result(connection_);
    if (res) {
      std::vector<const char*> outrow;
      MYSQL_ROW row;
      if ((row = mysql_fetch_row(res))) {
        unsigned int nfields = mysql_num_fields(res);
        outrow.resize(nfields);
        for (unsigned int i = 0; i < nfields; i++) {
          outrow[i] = row[i];
        }
      }
      if (outrow.empty()) {
        mysql_free_result(res);
        return nullptr;
      }
      return new RealResultRow(outrow, res);
    } else {
      std::stringstream ss;
      ss << "Error fetching query results: ";
      ss << mysql_error(connection_) << " (" << mysql_errno(connection_) << ")";
      throw Error(ss.str().c_str(), mysql_errno(connection_));
    }
  }
  throw Error("Not connected", 0);
}

uint64_t MySQLSession::last_insert_id() {
  return mysql_insert_id(connection_);
}

std::string MySQLSession::quote(const std::string &s) {
  std::string r;
  r.resize(s.length()*2+3);
  r[0] = '\'';
  unsigned long len = mysql_real_escape_string_quote(connection_, &r[1],
                                                    s.c_str(), s.length(), '\'');
  r.resize(len+2);
  r[len+1] = '\'';
  return r;
}
