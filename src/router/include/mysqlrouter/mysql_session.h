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

#ifndef _ROUTER_MYSQL_SESSION_H_
#define _ROUTER_MYSQL_SESSION_H_

#include <string>
#include <vector>
#include <stdexcept>
#include <functional>

struct st_mysql;

namespace mysqlrouter {

class MySQLSession {
public:
  class Transaction {
  public:
    Transaction(MySQLSession *session) : session_(session) {
      session_->execute("START TRANSACTION");
    }

    ~Transaction() {
      if (session_)
        session_->execute("ROLLBACK");
    }

    void commit() {
      session_->execute("COMMIT");
      session_ = nullptr;
    }

    void rollback() {
      session_->execute("ROLLBACK");
      session_ = nullptr;
    }

  private:
    MySQLSession *session_;
  };

  static const int kDefaultConnectionTimeout = 15;

  class Error : public std::runtime_error {
  public:
    Error(const char *error, unsigned int code)
    : std::runtime_error(error), code_(code) {}

    unsigned int code() const { return code_; }
  private:
    unsigned int code_;
  };

  typedef std::vector<const char*> Row;
  typedef std::function<bool (const Row&)> RowProcessor;

  class ResultRow {
  public:
    virtual ~ResultRow() {}
    size_t size() const { return row_.size(); }
    const char *operator[] (size_t i) { return row_[i]; }
  protected:
    Row row_;
  };

  MySQLSession();
  virtual ~MySQLSession();

  virtual void connect(const std::string &host, unsigned int port,
                       const std::string &username,
                       const std::string &password,
                       int connection_timeout = kDefaultConnectionTimeout);
  virtual void disconnect();

  virtual void execute(const std::string &query);
  virtual void query(const std::string &query, const RowProcessor &processor);
  ResultRow *query_one(const std::string &query);

  uint64_t last_insert_id();

  virtual std::string quote(const std::string &s);
private:
  st_mysql *connection_;
  bool connected_;
};

}
#endif
