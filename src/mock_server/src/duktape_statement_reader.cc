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

#include <string>
#include <map>
#include <functional>

#include "duktape.h"
#include "duk_logging.h"
#include "duk_module_shim.h"
#include "duktape_statement_reader.h"
#include "duk_node_fs.h"
#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace server_mock {

struct DuktapeStatementReader::Pimpl {
  std::string get_object_string_value(duk_idx_t idx,
      const std::string &field,
      const std::string &default_val = "",
      bool is_required = false) {
    std::string value;

    duk_get_prop_string(ctx, idx, field.c_str());

    if (duk_is_undefined(ctx, -1)) {
      if (is_required) {
        throw std::runtime_error("Wrong statements document structure: missing field\"" + field  + "\"");
      }

      value = default_val;
    } else {
      value = duk_to_string(ctx, -1);
    }

    duk_pop(ctx);

    return value;
  }

  template<class INT_TYPE>
  INT_TYPE get_object_integer_value(duk_idx_t idx,
                                    const std::string& field,
                                    const INT_TYPE default_val = 0,
                                    bool is_required = false) {
    INT_TYPE value;

    duk_get_prop_string(ctx, idx, field.c_str());

    if (duk_is_undefined(ctx, -1)) {
      if (is_required) {
        throw std::runtime_error("Wrong statements document structure: missing field\"" + field  + "\"");
      }

      value = default_val;
    } else {
      value = duk_to_int(ctx, -1);
    }

    duk_pop(ctx);

    return value;
  }

  std::unique_ptr<Response> get_ok(duk_idx_t idx) {
    if (!duk_is_object(ctx, idx)) {
      throw std::runtime_error("expect a object");
    }

    unsigned int last_insert_id = 0;
    unsigned int warning_count = 0;

    if (duk_get_prop_string(ctx, -1, "last_insert_id")) {
      last_insert_id = duk_require_int(ctx, -1);
    }
    duk_pop(ctx);

    if (duk_get_prop_string(ctx, -1, "warning_count")) {
      warning_count = duk_require_int(ctx, -1);
    }
    duk_pop(ctx);

    return std::unique_ptr<Response>(new OkResponse(last_insert_id, warning_count));
  }


  std::unique_ptr<Response> get_error(duk_idx_t idx) {
    if (!duk_is_object(ctx, idx)) {
      throw std::runtime_error("expect a object");
    }

    std::string sql_state;
    std::string msg;
    unsigned int code = 1149;

    if (duk_get_prop_string(ctx, -1, "sql_state")) {
      sql_state = duk_require_string(ctx, -1);
    }
    duk_pop(ctx);

    if (duk_get_prop_string(ctx, -1, "message")) {
      msg = duk_require_string(ctx, -1);
    }
    duk_pop(ctx);

    if (duk_get_prop_string(ctx, -1, "code")) {
      code = duk_require_int(ctx, -1);
    }
    duk_pop(ctx);

    return std::unique_ptr<Response>(new ErrorResponse(code, msg, sql_state));
  }

  std::unique_ptr<Response> get_result(duk_idx_t idx) {
    std::unique_ptr<ResultsetResponse> response(new ResultsetResponse);
    if (!duk_is_object(ctx, idx)) {
      throw std::runtime_error("expect a object");
    }
    duk_get_prop_string(ctx, idx, "columns");

    if (!duk_is_array(ctx, idx)) {
      throw std::runtime_error("expect a object");
    }
    // iterate over the column meta
    duk_enum(ctx, -1, DUK_ENUM_ARRAY_INDICES_ONLY);
    while (duk_next(ctx, -1, 1)) {
      // @-2 column-ndx
      // @-1 column
      RowValueType row_values;

      column_info_type column_info {
          get_object_string_value(-1, "name", "", true),
          column_type_from_string(get_object_string_value(-1, "type", "", true)),
          get_object_string_value(-1, "orig_name"),
          get_object_string_value(-1, "table"),
          get_object_string_value(-1, "orig_table"),
          get_object_string_value(-1, "schema"),
          get_object_string_value(-1, "catalog", "def"),
          get_object_integer_value<uint16_t>(-1, "flags"),
          get_object_integer_value<uint8_t>(-1, "decimals"),
          get_object_integer_value<uint32_t>(-1, "length"),
          get_object_integer_value<uint16_t>(-1, "character_set", 63),
          get_object_integer_value<unsigned>(-1, "repeat", 1)
      };

      response->columns.push_back(column_info);

      duk_pop(ctx); // row
      duk_pop(ctx); // row-ndx
    }
    duk_pop(ctx); // rows-enum

    duk_pop(ctx);
    duk_get_prop_string(ctx, idx, "rows");

    // object|undefined
    if (duk_is_object(ctx, -1)) {
      // no rows

      duk_enum(ctx, -1, DUK_ENUM_ARRAY_INDICES_ONLY);
      while (duk_next(ctx, -1, 1)) {
        // @-2 row-ndx
        // @-1 row
        RowValueType row_values;

        duk_enum(ctx, -1, DUK_ENUM_ARRAY_INDICES_ONLY);
        while (duk_next(ctx, -1, 1)) {
          if (duk_is_null(ctx, -1)) {
            row_values.push_back(std::make_pair(false, ""));
          } else {
            row_values.push_back(std::make_pair(true, duk_to_string(ctx, -1)));
          }
          duk_pop(ctx); // field
          duk_pop(ctx); // field-ndx
        }
        duk_pop(ctx); // field-enum
        response->rows.push_back(row_values);

        duk_pop(ctx); // row
        duk_pop(ctx); // row-ndx
      }
      duk_pop(ctx); // rows-enum
    } else if (!duk_is_undefined(ctx, -1)) {
      log_warning("rows: expected array or undefined, got something else. Ignoring");
    }

    duk_pop(ctx); // "rows"

    return response;
  }
  duk_context *ctx {nullptr};
};

duk_int_t duk_peval_file(duk_context *ctx, const char *path) {
  duk_push_c_function(ctx, duk_node_fs_read_file_sync, 1);
  duk_push_string(ctx, path);
  if (duk_int_t rc = duk_pcall(ctx, 1)) {
    return rc;
  }

  duk_buffer_to_string(ctx, -1);
  duk_push_string(ctx, path);
  if (duk_int_t rc = duk_pcompile(ctx, DUK_COMPILE_EVAL)) {
    return rc;
  }
  duk_push_global_object(ctx);
  return duk_pcall_method(ctx, 0);
}

static
duk_int_t process_get_shared(duk_context *ctx) {
  const char *key = duk_require_string(ctx, 0);

  duk_push_global_stash(ctx);
  duk_get_prop_string(ctx, -1, "shared");
  auto *shared_globals = static_cast<MockServerGlobalScope *>(duk_get_pointer(ctx, -1));

  auto v = shared_globals->get_all();

  auto it = v.find(key);
  if (it == v.end()) {
    duk_push_undefined(ctx);
  } else {
    auto value = (*it).second;
    duk_push_lstring(ctx, value.c_str(), value.size());
    duk_json_decode(ctx, -1);
  }

  duk_remove(ctx, -2); // 'shared' pointer
  duk_remove(ctx, -2); // global stash

  return 1;
}

static
duk_int_t process_set_shared(duk_context *ctx) {
  const char *key = duk_require_string(ctx, 0);
  duk_require_valid_index(ctx, 1);

  duk_push_global_stash(ctx);
  duk_get_prop_string(ctx, -1, "shared");
  auto *shared_globals = static_cast<MockServerGlobalScope *>(duk_get_pointer(ctx, -1));

  if (nullptr == shared_globals) {
    return duk_generic_error(ctx, "shared is null");
  }

  duk_dup(ctx, 1);
  shared_globals->set(key, duk_json_encode(ctx, -1));

  duk_pop(ctx); // the dup
  duk_pop(ctx); // 'shared' pointer
  duk_pop(ctx); // global

  return 0;
}

/**
 * dismissable scope guard.
 *
 * used with RAII to call cleanup function if not dismissed
 *
 * allows to release resources in case exceptions are thrown
 */
class ScopeGuard {
  std::function<void()> undo_func_;
public:
  template<class Callable>
  ScopeGuard(Callable &&undo_func):
    undo_func_{std::forward<Callable>(undo_func)}
  {
  }

  void dismiss() {
    undo_func_ = nullptr;
  }
  ~ScopeGuard() {
    if (undo_func_) undo_func_();
  }
};


DuktapeStatementReader::DuktapeStatementReader(
    const std::string &filename,
    const std::string &module_prefix,
    std::shared_ptr<MockServerGlobalScope> shared_globals):
  pimpl_{new Pimpl()},
  shared_{shared_globals}
{
  auto *ctx = duk_create_heap_default();

  // free the duk_context if an exception gets thrown as DuktapeStatementReaders's destructor
  // will not be called in that case.
  ScopeGuard duk_guard{[&ctx](){
    duk_destroy_heap(ctx);
  }};

  // init module-loader
  duk_module_shim_init(ctx, module_prefix.c_str());

  duk_push_global_stash(ctx);
  if (nullptr == shared_.get()) {
    throw std::logic_error("what is going one?");
  }

  duk_push_pointer(ctx, shared_.get());
  duk_put_prop_string(ctx, -2, "shared");
  duk_pop(ctx); // stash

  duk_get_global_string(ctx, "process");
  if (duk_is_undefined(ctx, -1)) {
    throw std::runtime_error("...");
  }
  duk_push_c_function(ctx, process_get_shared, 1);
  duk_put_prop_string(ctx, -2, "get_shared");

  duk_push_c_function(ctx, process_set_shared, 2);
  duk_put_prop_string(ctx, -2, "set_shared");

  duk_pop(ctx);

  duk_push_global_object(ctx);
  duk_push_object(ctx);
  duk_push_object(ctx);

  duk_push_int(ctx, 3306); // TODO gets the mock's bound port
  duk_put_prop_string(ctx, -2, "port");

  duk_put_prop_string(ctx, -2, "session");

  duk_ret_t rc = duk_pcompile_string(ctx, DUK_COMPILE_FUNCTION,
      "function () {\n"
      "  return new Proxy({}, {\n"
      "    get: function(targ, key, recv) {return process.get_shared(key);},\n"
      "    set: function(targ, key, val, recv) {return process.set_shared(key, val);}\n"
      "  });\n"
      "}");
  if (rc != DUK_EXEC_SUCCESS) {
    duk_get_prop_string(ctx, -1, "stack");
    duk_pop(ctx);

    throw std::runtime_error("...");
  }
  rc = duk_pcall(ctx, 0);
  if (rc != DUK_EXEC_SUCCESS) {
    duk_get_prop_string(ctx, -1, "stack");
    duk_pop(ctx);

    throw std::runtime_error("...");
  }

  duk_put_prop_string(ctx, -2, "global");

  duk_put_prop_string(ctx, -2, "mysqld");

  if (DUK_EXEC_SUCCESS != duk_peval_file(ctx, filename.c_str())) {
    if (duk_is_error(ctx, -1)) {
      duk_get_prop_string(ctx, -1, "stack");
      std::string err_stack { duk_safe_to_string(ctx, -1) };
      duk_pop(ctx);
      duk_get_prop_string(ctx, -1, "fileName");
      std::string err_filename { duk_safe_to_string(ctx, -1) };
      duk_pop(ctx);
      duk_get_prop_string(ctx, -1, "lineNumber");
      std::string err_fileline { duk_safe_to_string(ctx, -1) };
      duk_pop(ctx);

      throw std::runtime_error("at " + err_filename + ":" + err_fileline + ": " + err_stack);
    } else {
      std::string err_message{duk_safe_to_string(ctx, -1)};

      throw std::runtime_error(err_message);
    }
  }

  if (!duk_is_object(ctx, -1)) {
    throw std::runtime_error(filename + ": expected statement handler to return an object");
  }
  duk_get_prop_string(ctx, -1, "stmts");
  if (duk_is_undefined(ctx, -1)) {
    duk_pop(ctx);

    throw std::runtime_error("has no 'stmts'");
  }

  // TODO: allow function too
  // TODO: be strict on what we accept here
  //       - enumable
  //       - thread
  //       - function
  if (!duk_is_thread(ctx, -1)) {
    duk_enum(ctx, -1, DUK_ENUM_ARRAY_INDICES_ONLY);
  }

  // we are still alive, dismiss the guard
  pimpl_->ctx = ctx;
  duk_guard.dismiss();
}

DuktapeStatementReader::~DuktapeStatementReader() {
  // duk_pop(pimpl_->ctx);

  if (pimpl_->ctx) duk_destroy_heap(pimpl_->ctx);
}

StatementAndResponse DuktapeStatementReader::handle_statement(const std::string &statement) {
  auto *ctx = pimpl_->ctx;
  bool is_thread = false;

  if (duk_is_thread(ctx, -1)) {
    is_thread = true;
    int rc = duk_pcompile_string(ctx, DUK_COMPILE_FUNCTION, "function (t, stmt) { return Duktape.Thread.resume(t, stmt); }");
    if (DUK_EXEC_SUCCESS != rc) {
      if (duk_is_error(ctx, -1)) {
        duk_get_prop_string(ctx, -1, "stack");
        std::string err_stack { duk_safe_to_string(ctx, -1) };
        duk_pop(ctx);
        duk_get_prop_string(ctx, -1, "fileName");
        std::string err_filename { duk_safe_to_string(ctx, -1) };
        duk_pop(ctx);
        duk_get_prop_string(ctx, -1, "lineNumber");
        std::string err_fileline { duk_safe_to_string(ctx, -1) };
        duk_pop(ctx);

        throw std::runtime_error("at " + err_filename + ":" + err_fileline + ": " + err_stack);
      } else {
        std::string err_msg { duk_safe_to_string(ctx, -1) };

        throw std::runtime_error(err_msg);
      }
    }
    if (!duk_is_thread(ctx, -2)) {
      throw std::runtime_error("oops");
    }
    duk_dup(ctx, -2); // the thread
    duk_push_lstring(ctx, statement.c_str(), statement.size());

    rc = duk_pcall(ctx, 2);
    if (DUK_EXEC_SUCCESS != rc) {
      if (duk_is_error(ctx, -1)) {
        duk_get_prop_string(ctx, -1, "stack");
        std::string err_stack { duk_safe_to_string(ctx, -1) };
        duk_pop(ctx);
        duk_get_prop_string(ctx, -1, "fileName");
        std::string err_filename { duk_safe_to_string(ctx, -1) };
        duk_pop(ctx);
        duk_get_prop_string(ctx, -1, "lineNumber");
        std::string err_fileline { duk_safe_to_string(ctx, -1) };
        duk_pop(ctx);

        throw std::runtime_error("at " + err_filename + ":" + err_fileline + ": " + err_stack);
      } else {
        std::string err_msg { duk_safe_to_string(ctx, -1) };

        throw std::runtime_error(err_msg);
      }
    }
    // @-1 result of resume
  } else {
    // @-1 is an enumarator
    if (0 == duk_next(ctx, -1, true)) {
      duk_pop(ctx);
      return {};
    }
    // @-3 is an enumarator
    // @-2 is key
    // @-1 is value
  }

  // value must be an object
  if (!duk_is_object(ctx, -1)) {
    throw std::runtime_error("expected a object, got " + std::to_string(duk_get_type(ctx, -1)));
  }

  StatementAndResponse response;
  duk_get_prop_string(ctx, -1, "exec_time");
  if (!duk_is_undefined(ctx, -1)) {
    if (!duk_is_number(ctx, -1)) {
      throw std::runtime_error("exec_time must be a number, if set");
    }

    double exec_time = duk_get_number(ctx, -1);;
    response.exec_time = std::chrono::microseconds(static_cast<long>(exec_time * 1000));
  }
  duk_pop(ctx);


  duk_get_prop_string(ctx, -1, "result");
  if (!duk_is_undefined(ctx, -1)) {
    response.response_type = StatementAndResponse::StatementResponseType::STMT_RES_RESULT;
    response.response = pimpl_->get_result(-1);
  } else {
    duk_pop(ctx); // result
    duk_get_prop_string(ctx, -1, "error");
    if (!duk_is_undefined(ctx, -1)) {
      response.response_type = StatementAndResponse::StatementResponseType::STMT_RES_ERROR;
      response.response = pimpl_->get_error(-1);
    } else {
      duk_pop(ctx); // error
      duk_get_prop_string(ctx, -1, "ok");
      if (!duk_is_undefined(ctx, -1)) {
        response.response_type = StatementAndResponse::StatementResponseType::STMT_RES_OK;
        response.response = pimpl_->get_ok(-1);
      } else {
        throw std::runtime_error("expected 'error', 'ok' or 'result'");
      }
    }
  }
  duk_pop(ctx); // last prop

  duk_pop(ctx); // value
  if (!is_thread) {
    duk_pop(ctx); // key
  }

  return response;
}

std::chrono::microseconds DuktapeStatementReader::get_default_exec_time() {
  return std::chrono::microseconds{0};
}

}
