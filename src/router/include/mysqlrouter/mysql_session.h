/*
  Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql.h>  // enum mysql_ssl_mode

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

  // text representations of SSL modes
  static const char kSslModeDisabled[];
  static const char kSslModePreferred[];
  static const char kSslModeRequired[];
  static const char kSslModeVerifyCa[];
  static const char kSslModeVerifyIdentity[];

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

  static mysql_ssl_mode parse_ssl_mode(std::string ssl_mode); // throws std::logic_error
  static const char* ssl_mode_to_string(mysql_ssl_mode ssl_mode) noexcept;

  virtual void set_ssl_options(mysql_ssl_mode ssl_mode,
                               const std::string &tls_version,
                               const std::string &ssl_cipher,
                               const std::string &ca, const std::string &capath,
                               const std::string &crl, const std::string &crlpath);         // throws Error
  virtual void set_ssl_cert(const std::string &cert, const std::string &key);

  virtual void connect(const std::string &host, unsigned int port,
                       const std::string &username,
                       const std::string &password,
                       const std::string &unix_socket,
                       const std::string &default_schema,
                       int connection_timeout = kDefaultConnectionTimeout); // throws Error
  virtual void disconnect();

  virtual void execute(const std::string &query); // throws Error, std::logic_error
  virtual void query(const std::string &query, const RowProcessor &processor);  // throws Error, std::logic_error
  virtual ResultRow *query_one(const std::string &query); // throws Error

  virtual uint64_t last_insert_id() noexcept;

  virtual std::string quote(const std::string &s, char qchar = '\'') noexcept;

  virtual bool is_connected() noexcept { return connection_ && connected_; }
  const std::string& get_address() noexcept { return connection_address_; }

  virtual const char *last_error();
  virtual unsigned int last_errno();

private:
  st_mysql *connection_;
  bool connected_;
  std::string connection_address_;

  virtual st_mysql* raw_mysql() noexcept { return connection_; }
  static bool check_for_yassl(st_mysql *connection);

  #ifdef FRIEND_TEST
  friend class ::MockMySQLSession;
  #endif
};

} // namespace mysqlrouter

#endif
