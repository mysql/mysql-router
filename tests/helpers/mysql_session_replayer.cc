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

#include "mysql_session_replayer.h"
#include "mysqlrouter/utils_sqlstring.h"
#include <iostream>

using mysqlrouter::MySQLSession;

MySQLSessionReplayer::MySQLSessionReplayer(bool trace) : trace_(trace) {
}

MySQLSessionReplayer::~MySQLSessionReplayer() {

}

void MySQLSessionReplayer::connect(const std::string &, unsigned int,
                                   const std::string &,
                                   const std::string &,
                                   int) {
}

void MySQLSessionReplayer::disconnect() {

}

void MySQLSessionReplayer::execute(const std::string &query) {
  if (call_info_.empty())
    throw std::logic_error("Unexpected call to execute("+query+")");
  const CallInfo &info(call_info_.front());
  if (query.compare(0, info.query.length(), info.query) != 0
      || info.type != CallInfo::Execute) {
    throw std::logic_error("Unexpected/out-of-order call to execute("+query+")\nExpected: "+info.query);
  }
  last_insert_id_ = info.last_insert_id;
  if (trace_)
    std::cout << "execute: " << query << "\n";
  if (info.error_code != 0) {
    call_info_.pop_front();
    throw MySQLSession::Error(info.error.c_str(), info.error_code);
  }
  call_info_.pop_front();
}

void MySQLSessionReplayer::query(const std::string &query, const RowProcessor &processor) {
  if (call_info_.empty())
    throw std::logic_error("Unexpected call to query("+query+")");
  const CallInfo &info(call_info_.front());
  if (query.compare(0, info.query.length(), info.query) != 0
      || info.type != CallInfo::Query) {
    throw std::logic_error("Unexpected/out-of-order call to query("+query+")\nExpected: "+info.query);
  }
  if (trace_)
    std::cout << "query: " << query << "\n";

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

MySQLSession::ResultRow *MySQLSessionReplayer::query_one(const std::string &query) {
  if (call_info_.empty())
    throw std::logic_error("Unexpected call to query_one("+query+")");
  const CallInfo &info(call_info_.front());
  if (query.compare(0, info.query.length(), info.query) != 0
      || info.type != CallInfo::QueryOne) {
    throw std::logic_error("Unexpected/out-of-order call to query_one("+query+")\nExpected: "+info.query);
  }
  if (trace_)
    std::cout << "query_one: " << query << "\n";

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

std::string MySQLSessionReplayer::quote(const std::string &s, char qchar) {
  std::string quoted;
  quoted.push_back(qchar);
  quoted.append(mysqlrouter::escape_sql_string(s));
  quoted.push_back(qchar);
  return quoted;
}

MySQLSessionReplayer &MySQLSessionReplayer::expect_execute(const std::string &q) {
  CallInfo call;
  call.type = CallInfo::Execute;
  call.query = q;
  call_info_.push_back(call);
  return *this;
}

MySQLSessionReplayer &MySQLSessionReplayer::expect_query(const std::string &q) {
  CallInfo call;
  call.type = CallInfo::Query;
  call.query = q;
  call_info_.push_back(call);
  return *this;
}

MySQLSessionReplayer &MySQLSessionReplayer::expect_query_one(const std::string &q) {
  CallInfo call;
  call.type = CallInfo::QueryOne;
  call.query = q;
  call_info_.push_back(call);
  return *this;
}

void MySQLSessionReplayer::then_ok(uint64_t last_insert_id) {
  call_info_.back().last_insert_id = last_insert_id;
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

void MySQLSessionReplayer::print_expected() {
  std::cout << "Expected MySQLSession calls:\n";
  for (auto &info : call_info_) {
    switch (info.type) {
      case CallInfo::Execute:
        std::cout << "\texecute: ";
        break;
      case CallInfo::Query:
        std::cout << "\tquery: ";
        break;
      case CallInfo::QueryOne:
        std::cout << "\tquery_one: ";
        break;
    }
    std::cout << info.query << "\n";
  }
}
