/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "json_statement_reader.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE globally
// and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/error/en.h"
#include <cassert>
#include <chrono>
#include <memory>

namespace {

// default allocator for rapidJson (MemoryPoolAllocator) is broken for SparcSolaris
using JsonDocument = rapidjson::GenericDocument<rapidjson::UTF8<>,  rapidjson::CrtAllocator>;
using JsonValue = rapidjson::GenericValue<rapidjson::UTF8<>,  rapidjson::CrtAllocator>;

std::string get_json_value_as_string(const JsonValue& value, size_t repeat = 1) {
  if (value.IsString()) {
    const std::string val = value.GetString();
    std::string result;
    if (val.empty()) return val;
    result.reserve(val.length() * repeat);

    for (size_t i = 0; i < repeat; ++i) {
      result += val;
    }

    return result;
  }
  if (value.IsNull()) return "";
  if (value.IsInt()) return std::to_string(value.GetInt());
  if (value.IsUint()) return std::to_string(value.GetUint());
  if (value.IsDouble()) return std::to_string(value.GetDouble());
  // TODO: implement other types when needed

  throw(std::runtime_error("Unsupported json value type: "
                           + std::to_string(static_cast<int>(value.GetType()))));
}

std::string get_json_string_field(const JsonValue& parent,
                                  const std::string& field,
                                  const std::string& default_val = "",
                                  bool required = false) {
  const bool found = parent.HasMember(field.c_str());
  if (!found) {
    if (required) {
      throw std::runtime_error("Wrong statements document structure: missing field\"" + field  + "\"");
    }
    return default_val;
  }

  if (!parent[field.c_str()].IsString()) {
    throw std::runtime_error("Wrong statements document structure: field\"" + field  + "\" has to be string type");
  }

  return parent[field.c_str()].GetString();
}

double get_json_double_field(const JsonValue& parent,
                             const std::string& field,
                             const double default_val = 0.0,
                             bool required = false) {
  const bool found = parent.HasMember(field.c_str());
  if (!found) {
    if (required) {
      throw std::runtime_error("Wrong statements document structure: field\"" + field + "\"");
    }
    return default_val;
  }

  if (!parent[field.c_str()].IsDouble()) {
    throw std::runtime_error("Wrong statements document structure: field\"" + field + "\" has to be double type");
  }
  return parent[field.c_str()].GetDouble();
}

template<class INT_TYPE>
INT_TYPE get_json_integer_field(const JsonValue& parent,
                                  const std::string& field,
                                  const INT_TYPE default_val = 0,
                                  bool required = false) {
  const bool found = parent.HasMember(field.c_str());
  if (!found) {
    if (required) {
      throw std::runtime_error("Wrong statements document structure: missing field\"" + field  + "\"");
    }
    return default_val;
  }

  if (!parent[field.c_str()].IsInt()) {
    throw std::runtime_error("Wrong statements document structure: field\"" + field  + "\" has to be integer type");
  }

  return static_cast<INT_TYPE>(parent[field.c_str()].GetInt());
}

} // namspace {}

namespace server_mock {

struct QueriesJsonReader::Pimpl {

  JsonDocument json_document_;
  size_t current_stmt_{0u};

  std::unique_ptr<Response> read_result_info(const JsonValue& stmt);

  std::unique_ptr<Response> read_ok_info(const JsonValue& stmt);

  std::unique_ptr<Response> read_error_info(const JsonValue& stmt);

  Pimpl(): json_document_() {}
};

QueriesJsonReader::QueriesJsonReader(const std::string &filename):
              pimpl_(new Pimpl()){
  if (filename.empty()) return;

#ifndef _WIN32
  const char* OPEN_MODE = "rb"; // after rapidjson doc
#else
  const char* OPEN_MODE = "r";
#endif
  FILE* fp = fopen(filename.c_str(), OPEN_MODE);
  if (!fp) {
    throw std::runtime_error("Could not open json queries file for reading: " + filename);
  }
  std::shared_ptr<void> exit_guard(nullptr, [&](void*){fclose(fp);});

  char readBuffer[65536];
  rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

  pimpl_->json_document_.ParseStream<rapidjson::kParseCommentsFlag>(is);

  if (pimpl_->json_document_.HasParseError()) {
    throw std::runtime_error("Parsing " + filename + " failed at pos " + std::to_string(pimpl_->json_document_.GetErrorOffset()) + ": "
        + RAPIDJSON_NAMESPACE::GetParseError_En(pimpl_->json_document_.GetParseError()));
  }

  if (!pimpl_->json_document_.HasMember("stmts")) {
    throw std::runtime_error("Wrong statements document structure: missing \"stmts\"");
  }

  if (!pimpl_->json_document_["stmts"].IsArray()) {
    throw std::runtime_error("Wrong statements document structure: \"stmts\" has to be an array");
  }

}

// this is needed for pimpl, otherwise compiler complains
// about pimpl unknown size in std::unique_ptr
QueriesJsonReader::~QueriesJsonReader() {}

StatementAndResponse QueriesJsonReader::get_next_statement() {
  StatementAndResponse response;

  const JsonValue& stmts = pimpl_->json_document_["stmts"];
  if (pimpl_->current_stmt_ >= stmts.Size()) return response;

  auto& stmt = stmts[pimpl_->current_stmt_++];
  if (!stmt.HasMember("stmt") && !stmt.HasMember("stmt.regex")) {
    throw std::runtime_error("Wrong statements document structure: missing \"stmt\" or \"stmt.regex\"");
  }

  if (stmt.HasMember("exec-time")) {
    double exec_time = get_json_double_field(stmt, "exec-time", 0.0);
    response.exec_time = std::chrono::microseconds(static_cast<long>(exec_time * 1000));
  }
  else {
    response.exec_time = get_default_exec_time();
  }

  std::string name{"stmt"};
  if (stmt.HasMember("stmt.regex")) {
    name = "stmt.regex";
    response.statement_is_regex = true;
  }

  if (!stmt[name.c_str()].IsString()) {
    throw std::runtime_error("Wrong statements document structure: \"stmt\" has to be a string");
  }

  response.statement = stmt[name.c_str()].GetString();

  if (stmt.HasMember("ok")) {
    response.response_type = StatementAndResponse::StatementResponseType::STMT_RES_OK;
    response.response = pimpl_->read_ok_info(stmt);
  } else if (stmt.HasMember("error")) {
    response.response_type = StatementAndResponse::StatementResponseType::STMT_RES_ERROR;
    response.response = pimpl_->read_error_info(stmt);
  } else if (stmt.HasMember("result")) {
    response.response_type = StatementAndResponse::StatementResponseType::STMT_RES_RESULT;
    response.response = pimpl_->read_result_info(stmt);
  } else {
    throw std::runtime_error("Wrong statements document structure: expect \"ok|error|result\"");
  }

  return response;
}

std::chrono::microseconds QueriesJsonReader::get_default_exec_time() {

  if (pimpl_->json_document_.HasMember("defaults")) {
    auto& defaults = pimpl_->json_document_["defaults"];
    if (defaults.HasMember("exec-time")) {
      double exec_time = get_json_double_field(defaults, "exec-time", 0.0);
      return std::chrono::microseconds(static_cast<long>(exec_time * 1000));
    }
  }
  return std::chrono::microseconds(0);
}

std::unique_ptr<Response> QueriesJsonReader::Pimpl::read_result_info(const JsonValue &stmt) {
  // only asserting as this should have been checked before if we got here
  assert(stmt.HasMember("result"));

  const auto& result = stmt["result"];

  std::unique_ptr<ResultsetResponse> response(new ResultsetResponse);

  // read columns
  if (result.HasMember("columns")) {
    const auto& columns = result["columns"];
    if (!columns.IsArray()) {
      throw std::runtime_error("Wrong statements document structure: \"columns\" has to be an array");
    }

    for (size_t i = 0; i < columns.Size(); ++i) {
      auto& column = columns[i];
      column_info_type column_info {
          get_json_string_field(column, "name", "", true),
          column_type_from_string(get_json_string_field(column, "type", "", true)),
          get_json_string_field(column, "orig_name"),
          get_json_string_field(column, "table"),
          get_json_string_field(column, "orig_table"),
          get_json_string_field(column, "schema"),
          get_json_string_field(column, "catalog", "def"),
          get_json_integer_field<uint16_t>(column, "flags"),
          get_json_integer_field<uint8_t>(column, "decimals"),
          get_json_integer_field<uint32_t>(column, "length"),
          get_json_integer_field<uint16_t>(column, "character_set", 63),
          get_json_integer_field<unsigned>(column, "repeat", 1)
      };

      response->columns.push_back(column_info);
    }
  }

  // read rows
  if (result.HasMember("rows")) {
    const auto& rows = result["rows"];
    if (!rows.IsArray()) {
      throw std::runtime_error("Wrong statements document structure: \"rows\" has to be an array");
    }

    auto columns_size = response->columns.size();

    for (size_t i = 0; i < rows.Size(); ++i) {
      auto& row = rows[i];
      if (!row.IsArray()) {
        throw std::runtime_error("Wrong statements document structure: \"rows\" instance has to be an array");
      }

      if (row.Size() != columns_size) {
        throw std::runtime_error(std::string("Wrong statements document structure: ") +
            "number of row fields different than number of columns " +
            std::to_string(row.Size()) + " != " + std::to_string(columns_size));
      }

      RowValueType row_values;
      for (size_t j = 0; j < row.Size(); ++j) {
        auto& column_info = response->columns[j];
        const size_t repeat = static_cast<size_t>(column_info.repeat);
        if (row[j].IsNull()) {
          row_values.push_back(std::make_pair(false, ""));
        } else {
          row_values.push_back(std::make_pair(true, get_json_value_as_string(row[j], repeat)));
        }
      }

      response->rows.push_back(row_values);
    }
  }

  return std::move(response);
}

std::unique_ptr<Response> QueriesJsonReader::Pimpl::read_ok_info(const JsonValue &stmt) {
  // only asserting as this should have been checked before if we got here
  assert(stmt.HasMember("ok"));

  const auto& f_ok = stmt["ok"];

  return std::unique_ptr<Response>(new OkResponse(
    get_json_integer_field<unsigned int>(f_ok, "last_insert_id", 0),
    get_json_integer_field<unsigned int>(f_ok, "warnings", 0)));
}

std::unique_ptr<Response> QueriesJsonReader::Pimpl::read_error_info(const JsonValue &stmt) {
  // only asserting as this should have been checked before if we got here
  assert(stmt.HasMember("error"));

  const auto& f_error = stmt["error"];

  return std::unique_ptr<Response>(new ErrorResponse(
    get_json_integer_field<unsigned int>(f_error, "code", 0, true),
    get_json_string_field(f_error, "message", "unknown error-msg"),
    get_json_string_field(f_error, "sql_state", "HY000")));
}



} // namespace server_mock
