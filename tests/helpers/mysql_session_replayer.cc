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

#include "mysql_session_replayer.h"
#include "mysqlrouter/utils_sqlstring.h"
#include <iostream>

using mysqlrouter::MySQLSession;

MySQLSessionReplayer::MySQLSessionReplayer(bool trace) : trace_(trace) {
}

MySQLSessionReplayer::~MySQLSessionReplayer() {

}

void MySQLSessionReplayer::connect(const std::string &host, unsigned int port,
                                   const std::string &user,
                                   const std::string &password,
                                   int) {
  if (trace_) {
    std::cout << "connect: " << user << ":" << password << "@" << host << ":" << port << "\n";
  }

  // check if connect() is not expected to fail. Note that since we mostly just
  // connect() without errors and go on about our business, connect() is allowed
  // to be called without a prior call to expect_connect(). This is in contrast to
  // execute(), query() and friends, which must be preceded by their respective
  // expect_*() call.
  if (call_info_.size()
      && call_info_.front().type == CallInfo::Connect
      && call_info_.front().host == host
      && call_info_.front().port == port
      && call_info_.front().user == user
      && call_info_.front().password == password) {
    CallInfo info = call_info_.front();
    call_info_.pop_front();

    if (info.error_code != 0) {
      connected_ = false;
      throw MySQLSession::Error(info.error.c_str(), info.error_code);
    }
  }

  connected_ = true;
}

void MySQLSessionReplayer::disconnect() {
  connected_ = false;
}

void MySQLSessionReplayer::execute(const std::string &sql) {
  if (call_info_.empty()) {
    if (trace_)
      std::cout << "unexpected execute: " << sql << "\n";
    throw std::logic_error("Unexpected call to execute("+sql+")");
  }
  const CallInfo info(call_info_.front());
  if (sql.compare(0, info.sql.length(), info.sql) != 0
      || info.type != CallInfo::Execute) {
    if (trace_)
      std::cout << "wrong execute: " << sql << "\n";
    throw std::logic_error("Unexpected/out-of-order call to execute("+sql+")\nExpected: "+info.sql);
  }
  last_insert_id_ = info.last_insert_id;
  if (trace_)
    std::cout << "execute: " << sql << "\n";
  if (info.error_code != 0) {
    call_info_.pop_front();
    throw MySQLSession::Error(info.error.c_str(), info.error_code);
  }
  call_info_.pop_front();
}

void MySQLSessionReplayer::query(const std::string &sql, const RowProcessor &processor) {
  if (call_info_.empty()) {
    if (trace_)
      std::cout << "unexpected query: " << sql << "\n";
    throw std::logic_error("Unexpected call to query("+sql+")");
  }
  const CallInfo info(call_info_.front());
  if (sql.compare(0, info.sql.length(), info.sql) != 0
      || info.type != CallInfo::Query) {
    if (trace_)
      std::cout << "wrong query: " << sql << "\n";
    throw std::logic_error("Unexpected/out-of-order call to query("+sql+")\nExpected: "+info.sql);
  }
  if (trace_)
    std::cout << "query: " << sql << "\n";

  if (info.error_code != 0) {
    call_info_.pop_front();
    throw MySQLSession::Error(info.error.c_str(), info.error_code);
  }
  for (auto &row : info.rows) {
    Row r;
    for (auto &field : row) {
      if (field) {
        r.push_back(field.c_str());
      } else {
        r.push_back(nullptr);
      }
    }
    try {
      if (!processor(r))
        break;
    } catch (...) {
      last_insert_id_ = 0;
      call_info_.pop_front();
      throw;
    }
  }

  last_insert_id_ = 0;
  call_info_.pop_front();
}

class MyResultRow : public MySQLSession::ResultRow {
public:
  MyResultRow(const std::vector<MySQLSessionReplayer::string> &row)
    : real_row_(row) {

    for (auto &field : real_row_) {
      if (field) {
        row_.push_back(field.c_str());
      } else {
        row_.push_back(nullptr);
      }
    }
  }

private:
  std::vector<MySQLSessionReplayer::string> real_row_;
};

MySQLSession::ResultRow *MySQLSessionReplayer::query_one(const std::string &sql) {
  if (call_info_.empty()) {
    if (trace_)
      std::cout << "unexpected query_one: " << sql << "\n";
    throw std::logic_error("Unexpected call to query_one("+sql+")");
  }
  const CallInfo info(call_info_.front());
  if (sql.compare(0, info.sql.length(), info.sql) != 0
      || info.type != CallInfo::QueryOne) {
    if (trace_)
      std::cout << "unexpected query_one: " << sql << "\n";
    throw std::logic_error("Unexpected/out-of-order call to query_one("+sql+")\nExpected: "+info.sql);
  }
  if (trace_)
    std::cout << "query_one: " << sql << "\n";

  if (info.error_code != 0) {
    call_info_.pop_front();
    throw MySQLSession::Error(info.error.c_str(), info.error_code);
  }
  ResultRow *result = nullptr;
  if (!info.rows.empty()) {
    result = new MyResultRow(info.rows.front());
  }
  last_insert_id_ = 0;
  call_info_.pop_front();

  return result;
}

uint64_t MySQLSessionReplayer::last_insert_id() {
  return last_insert_id_;
}

const char *MySQLSessionReplayer::last_error() {
  return "some error";
}

unsigned int MySQLSessionReplayer::last_errno() {
  return 0;
}


std::string MySQLSessionReplayer::quote(const std::string &s, char qchar) {
  std::string quoted;
  quoted.push_back(qchar);
  quoted.append(mysqlrouter::escape_sql_string(s));
  quoted.push_back(qchar);
  return quoted;
}

MySQLSessionReplayer &MySQLSessionReplayer::expect_connect(const std::string &host, unsigned port,
                                                           const std::string &user,
                                                           const std::string &password) {
  CallInfo call;
  call.type = CallInfo::Connect;
  call.host = host;
  call.port = port;
  call.user = user;
  call.password = password;
  call_info_.push_back(call);
  return *this;
}

MySQLSessionReplayer &MySQLSessionReplayer::expect_execute(const std::string &q) {
  CallInfo call;
  call.type = CallInfo::Execute;
  call.sql = q;
  call_info_.push_back(call);
  return *this;
}

MySQLSessionReplayer &MySQLSessionReplayer::expect_query(const std::string &q) {
  CallInfo call;
  call.type = CallInfo::Query;
  call.sql = q;
  call_info_.push_back(call);
  return *this;
}

MySQLSessionReplayer &MySQLSessionReplayer::expect_query_one(const std::string &q) {
  CallInfo call;
  call.type = CallInfo::QueryOne;
  call.sql = q;
  call_info_.push_back(call);
  return *this;
}

void MySQLSessionReplayer::then_ok(uint64_t the_last_insert_id) {
  call_info_.back().last_insert_id = the_last_insert_id;
}

void MySQLSessionReplayer::then_error(const std::string &error, unsigned int code) {
  call_info_.back().error = error;
  call_info_.back().error_code = code;
}

void MySQLSessionReplayer::then_return(unsigned int num_fields,
                 std::vector<std::vector<string>> rows) {
  call_info_.back().num_fields = num_fields;
  call_info_.back().rows = rows;
}

bool MySQLSessionReplayer::print_expected() {
  std::cout << "Expected MySQLSession calls:\n";
  for (auto &info : call_info_) {
    switch (info.type) {
      case CallInfo::Execute:
        std::cout << "\texecute: ";
        std::cout << info.sql << "\n";
        break;
      case CallInfo::Query:
        std::cout << "\tquery: ";
        std::cout << info.sql << "\n";
        break;
      case CallInfo::QueryOne:
        std::cout << "\tquery_one: ";
        std::cout << info.sql << "\n";
        break;
      case CallInfo::Connect:
        std::cout << "\tconnect: ";
        std::cout << info.user << ":" << info.password << "@" << info.host << ":" << info.port << "\n";
        break;
    }
  }
  return !call_info_.empty();
}

MySQLSessionReplayer::CallInfo::CallInfo(const CallInfo& ci) : type(ci.type), error(ci.error), error_code(ci.error_code), sql(ci.sql),
last_insert_id(ci.last_insert_id), num_fields(ci.num_fields), rows(ci.rows), host(ci.host), port(ci.port), user(ci.user), password(ci.password)
{
}
