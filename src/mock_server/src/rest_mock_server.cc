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

/**
 */

#include <atomic>
#include <chrono>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/error/en.h>

// Harness interface include files
#include "mysql/harness/config_parser.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

#include "mysqlrouter/plugin_config.h"

#include "mysqlrouter/http_server_component.h"
#include "mysqlrouter/mock_server_component.h"

IMPORT_LOG_FUNCTIONS()

#ifdef _WIN32
  // workaround error C2039: 'GetObjectA': is not a member of ...
  //
  // as winnt.h #defines GetObject(...) GetObjectA(...)
  // and we call json_doc.GetObject() which gets replaced by the c-pre-processor
#  ifdef GetObject
#    undef GetObject
#  endif
#endif

static const char* kSectionName { "rest_mock_server" };

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::PluginFuncEnv;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;

class RestApiV1MockServer: public BaseRequestHandler {
  time_t last_modified;

public:
  RestApiV1MockServer():
    last_modified(time(nullptr)) {}

  // GET|PUT
  //
  void handle_request(HttpRequest &req) override {
    last_modified = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    if (!((HttpMethod::Get|HttpMethod::Put) & req.get_method())) {
      req.get_output_headers().add("Allow", "GET, PUT");
      req.send_reply(HttpStatusCode::MethodNotAllowed);
      return;
    }

    if (req.get_input_headers().get("Content-Range")) {
      req.send_reply(HttpStatusCode::NotImplemented);
      return;
    }

    if (HttpMethod::Get & req.get_method()) {
      if (!is_modified_since(req, last_modified)) {
        req.send_reply(HttpStatusCode::NotModified);
        return;
      }

      // GET
      add_last_modified(req, last_modified);

      auto chunk = req.get_output_buffer();

      {
        rapidjson::StringBuffer json_buf;
        {
          rapidjson::Writer<rapidjson::StringBuffer> json_writer(json_buf);
          rapidjson::Document json_doc;

          json_doc.SetObject();

          rapidjson::Document::AllocatorType& allocator = json_doc.GetAllocator();

          auto shared_globals = MockServerComponent::getInstance().getGlobalScope();
          auto all_globals = shared_globals->get_all();

          for (auto &element: all_globals) {
            rapidjson::Document value_doc;
            value_doc.Parse(element.second.c_str()); // value is a json-value as string

            if (value_doc.HasParseError()) {
              req.send_reply(HttpStatusCode::InternalError);
              return;
            }

            json_doc.AddMember(
                rapidjson::Value(element.first.c_str(), element.first.size(), allocator),
                value_doc,
                allocator);
          }

          json_doc.Accept(json_writer);
        } // free json_doc and json_writer early

        // perhaps we could use evbuffer_add_reference() and a unique-ptr on json_buf here.
        // needs to be benchmarked
        chunk.add(json_buf.GetString(), json_buf.GetSize());
      } // free json_buf early

      auto out_hdrs = req.get_output_headers();
      out_hdrs.add("Content-Type", "application/json");

      req.send_reply(HttpStatusCode::Ok, "Ok", chunk);
    } else if (HttpMethod::Put & req.get_method()) {
      const char *content_type = req.get_input_headers().get("Content-Type");
      // PUT
      //
      // required content-type: application/json
      if (nullptr == content_type || std::string(content_type) != "application/json") {
        req.send_reply(HttpStatusCode::UnsupportedMediaType);
        return;
      }
      auto body = req.get_input_buffer();
      auto data = body.pop_front(body.length());
      std::string str_data(data.begin(), data.end());

      rapidjson::Document body_doc;
      body_doc.Parse(str_data.c_str());

      if (body_doc.HasParseError()) {
        auto out_hdrs = req.get_output_headers();
        auto out_buf = req.get_output_buffer();
        out_hdrs.add("Content-Type", "text/plain");

        std::string parse_error(rapidjson::GetParseError_En(body_doc.GetParseError()));

        out_buf.add(parse_error.data(), parse_error.size());

        req.send_reply(HttpStatusCode::UnprocessableEntity, "Unprocessable Entity", out_buf);
        return;
      }

      if (!body_doc.IsObject()) {
        req.send_reply(HttpStatusCode::UnprocessableEntity);
        return;
      }

      // replace all the globals
      typename MockServerGlobalScope::type all_globals;

      for (auto& m : body_doc.GetObject()) {
        rapidjson::StringBuffer json_buf;
        rapidjson::Writer<rapidjson::StringBuffer> json_writer(json_buf);
        m.value.Accept(json_writer);

        all_globals[m.name.GetString()] = std::string(json_buf.GetString(), json_buf.GetSize());
      }

      auto shared_globals = MockServerComponent::getInstance().getGlobalScope();
      shared_globals->reset(all_globals);

      req.send_reply(HttpStatusCode::NoContent);
    }
  }
};

static void init(PluginFuncEnv* env) {
  const mysql_harness::AppInfo* info = get_app_info(env);

  if (nullptr == info->config) {
    return;
  }

  for (const mysql_harness::ConfigSection* section: info->config->sections()) {
    if (section->name != kSectionName) {
      continue;
    }

    // hmm, what to read from the config?
  }
}

static void start(PluginFuncEnv*) {
  auto &srv = HttpServerComponent::getInstance();

  srv.add_route("^/api/v1/mock_server/globals/$", std::unique_ptr<RestApiV1MockServer>(new RestApiV1MockServer()));
}

static void stop(PluginFuncEnv*) {
  auto &srv = HttpServerComponent::getInstance();

  srv.remove_route("^/api/v1/mock_server/globals/$");
}


#if defined(_MSC_VER) && defined(rest_mock_server_EXPORTS)
/* We are building this library */
#  define DLLEXPORT __declspec(dllexport)
#else
#  define DLLEXPORT
#endif

const char *plugin_requires[] = {
  "mock_server",
  "http_server",
};

extern "C" {
Plugin DLLEXPORT harness_plugin_rest_mock_server = {
  PLUGIN_ABI_VERSION,
  ARCHITECTURE_DESCRIPTOR,
  "REST_MOCK_SERVER",
  VERSION_NUMBER(0, 0, 1),
  sizeof(plugin_requires)/sizeof(plugin_requires[0]), plugin_requires,  // requires
  0, nullptr,  // conflicts
  init,        // init
  nullptr,     // deinit
  start,       // start
  stop,     // stop
};
}
