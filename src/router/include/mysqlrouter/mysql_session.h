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
#include <memory>

struct st_mysql;

#ifdef FRIEND_TEST
class MockMySQLSession;
#endif

namespace mysqlrouter {

class MySQLSession {
 public:
  static const int kDefaultConnectionTimeout = 15;
  typedef std::vector<const char*> Row;
  typedef std::function<bool (const Row&)> RowProcessor;

  class Transaction {
   public:
    Transaction(MySQLSession *session) : session_(session) {
      session_->execute("START TRANSACTION");
    }

    ~Transaction() {
      if (session_) {
        try {
          session_->execute("ROLLBACK");
        } catch (...) {
          // ignore errors during rollback on d-tor
        }
      }
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

  class Error : public std::runtime_error {
   public:
    Error(const char *error, unsigned int code__)
    : std::runtime_error(error), code_(code__) {}

    unsigned int code() const { return code_; }
   private:
    unsigned int code_;
  };

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
  virtual ResultRow *query_one(const std::string &query);

  virtual uint64_t last_insert_id();

  virtual std::string quote(const std::string &s, char qchar = '\'');

  virtual bool is_connected() { return connection_ && connected_; }
  const std::string& get_address() { return connection_address_; }

  virtual const char *last_error();
  virtual unsigned int last_errno();

private:
  st_mysql *connection_;
  bool connected_;
  std::string connection_address_;

  virtual st_mysql* raw_mysql() { return connection_; }

  #ifdef FRIEND_TEST
  friend class ::MockMySQLSession;
  #endif
};



/** @class MySQLSessionFactory
 *
 * This class is meant to ease use of DI (useful for unit testing). It is meant to be derived from
 * and its create() method to return a mock object, derived from MySQLSession.
 *
 * TODO: a better approach would probably be to move create() to a DI container, if/when we
 *       implement one.
 */
class MySQLSessionFactory {
 public:

  // it would seem to make more sense to use unique_ptr instead of shared_ptr here, but unfortunately
  // unique_ptr's deleter is part of its type specification.  Therefore all places where unique_ptr
  // was used would have to declare it like so: std::unique_ptr<MySQLSession, void(*)(MySQLSession*)>
  // This is not very convenient. shared_ptr doesn't have this problem.
  virtual std::shared_ptr<MySQLSession> create() const {
    // custom deleter guarantees that memory will be freed HERE. Which means, it won't get freed in another DLL
    return std::shared_ptr<MySQLSession>(new MySQLSession, [](MySQLSession* ptr) {
      delete ptr;
    });
  }

  virtual ~MySQLSessionFactory() {}
};

} // namespace mysqlrouter

#endif
