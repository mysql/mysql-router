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

#include "gmock/gmock.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"
#include "mysql_session.h"
#include "rapidjson/document.h"
#include "mysql/harness/logging/registry.h"
#include "dim.h"

#include "mysqlrouter/rest_client.h"

Path g_origin_path;

const std::string kMockServerGlobalsRestUri = "/api/v1/mock_server/globals/";
const std::string kMockServerInvalidRestUri = "/api/v1/mock_server/global/";

class RestMockServerTest : public RouterComponentTest, public ::testing::Test {
protected:
  TcpPortPool port_pool_;

  void SetUp() override {
    RouterComponentTest::SetUp();
  }
};

/**
 * test mock-server loaded the REST bridge.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, get_globals_empty) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("rest_server_mock.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  SCOPED_TRACE("// make a http connections");
  IOContext io_ctx;
  auto req = RestClient(io_ctx, http_hostname, http_port).
    request_sync(HttpMethod::GET, http_uri);

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req)
      << "HTTP Request to "
      << http_hostname << ":" << std::to_string(http_port)
      << " failed (early): "
      << req.error_msg()
      << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to "
      << http_hostname << ":" << std::to_string(http_port)
      << " failed: "
      << req.error_msg()
      << std::endl;

  EXPECT_EQ(req.get_response_code(), 200u);
  EXPECT_THAT(req.get_input_headers().get("Content-Type"), ::testing::StrEq("application/json"));

  auto resp_body = req.get_input_buffer();
  EXPECT_GT(resp_body.length(), 0u);
  auto resp_body_content = resp_body.pop_front(resp_body.length());

  // parse json

  std::string json_payload(resp_body_content.begin(), resp_body_content.end());

  rapidjson::Document json_doc;
  json_doc.Parse(json_payload.c_str());

  EXPECT_TRUE(!json_doc.HasParseError()) << json_payload;
}

/**
 * test mock-server's REST bridge denies unknown URLs.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, unknown_url_fails) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("rest_server_mock.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerInvalidRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  SCOPED_TRACE("// make a http connections");
  IOContext io_ctx;
  auto req = RestClient(io_ctx, http_hostname, http_port).
    request_sync(HttpMethod::GET, http_uri);

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req)
      << "HTTP Request to "
      << http_hostname << ":" << std::to_string(http_port)
      << " failed (early): "
      << req.error_msg()
      << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to "
      << http_hostname << ":" << std::to_string(http_port)
      << " failed: "
      << req.error_msg()
      << std::endl;

  EXPECT_EQ(req.get_response_code(), 404u);
  EXPECT_THAT(req.get_input_headers().get("Content-Type"), ::testing::StrEq("text/html"));

  auto resp_body = req.get_input_buffer();
  EXPECT_GT(resp_body.length(), 0u);
  auto resp_body_content = resp_body.pop_front(resp_body.length());
}

/**
 * test storing globals in mock_server via REST bridge.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, put_globals_no_json) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("rest_server_mock.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  SCOPED_TRACE("// make a http connections");
  IOContext io_ctx;
  auto req = RestClient(io_ctx, http_hostname, http_port).
    request_sync(HttpMethod::PUT, http_uri);

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req)
      << "HTTP Request to "
      << http_hostname << ":" << std::to_string(http_port)
      << " failed (early): "
      << req.error_msg()
      << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to "
      << http_hostname << ":" << std::to_string(http_port)
      << " failed: "
      << req.error_msg()
      << std::endl;

  EXPECT_EQ(req.get_response_code(), 415u);

  auto resp_body = req.get_input_buffer();
  EXPECT_EQ(resp_body.length(), 0u);
}

/**
 * test storing globals in mock_server via REST bridge.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, put_globals_ok) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("rest_server_mock.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  SCOPED_TRACE("// make a http connections");
  IOContext io_ctx;
  auto req = RestClient(io_ctx, http_hostname, http_port).
    request_sync(HttpMethod::PUT, http_uri, "{}");

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req)
      << "HTTP Request to "
      << http_hostname << ":" << std::to_string(http_port)
      << " failed (early): "
      << req.error_msg()
      << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to "
      << http_hostname << ":" << std::to_string(http_port)
      << " failed: "
      << req.error_msg()
      << std::endl;

  EXPECT_EQ(req.get_response_code(), 204u);

  auto resp_body = req.get_input_buffer();
  EXPECT_EQ(resp_body.length(), 0u);
}

/**
 * test storing globals in mock_server via REST bridge.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, put_globals_and_read_back) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("rest_server_mock.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  SCOPED_TRACE("// make a http connections");
  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port);
  auto put_req = rest_client.request_sync(HttpMethod::PUT, http_uri, "{\"key\": [ [1, 2, 3 ] ]}");

  SCOPED_TRACE("// checking PUT response");
  ASSERT_TRUE(put_req)
      << "HTTP Request to "
      << http_hostname << ":" << std::to_string(http_port)
      << " failed (early): "
      << put_req.error_msg()
      << std::endl;

  ASSERT_GT(put_req.get_response_code(), 0u)
      << "HTTP Request to "
      << http_hostname << ":" << std::to_string(http_port)
      << " failed: "
      << put_req.error_msg()
      << std::endl;

  EXPECT_EQ(put_req.get_response_code(), 204u);

  auto put_resp_body = put_req.get_input_buffer();
  EXPECT_EQ(put_resp_body.length(), 0u);

  // GET request

  auto get_req = rest_client.request_sync(HttpMethod::GET, http_uri);
  SCOPED_TRACE("// checking GET response");
  ASSERT_TRUE(get_req)
      << "HTTP Request to "
      << http_hostname << ":" << std::to_string(http_port)
      << " failed (early): "
      << get_req.error_msg()
      << std::endl;

  ASSERT_GT(get_req.get_response_code(), 0u)
      << "HTTP Request to "
      << http_hostname << ":" << std::to_string(http_port)
      << " failed: "
      << get_req.error_msg()
      << std::endl;

  EXPECT_EQ(get_req.get_response_code(), 200u);
  EXPECT_THAT(get_req.get_input_headers().get("Content-Type"), ::testing::StrEq("application/json"));

  auto get_resp_body = get_req.get_input_buffer();
  EXPECT_GT(get_resp_body.length(), 0u);
  auto get_resp_body_content = get_resp_body.pop_front(get_resp_body.length());

  // parse json

  std::string json_payload(get_resp_body_content.begin(), get_resp_body_content.end());

  rapidjson::Document json_doc;
  json_doc.Parse(json_payload.c_str());

  EXPECT_TRUE(!json_doc.HasParseError());
}


/**
 * test storing globals in mock_server via REST bridge.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, select_port) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("rest_server_mock.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  // mysql query
  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port, "username", "password", "", "")) << server_mock.get_full_output();

  std::unique_ptr<mysqlrouter::MySQLSession::ResultRow> result{
    client.query_one("select @@port")};
  ASSERT_NE(nullptr, result.get());
  ASSERT_EQ(1u, result->size());
  EXPECT_EQ(std::to_string(server_port), std::string((*result)[0]));
}

/**
 * test storing globals in mock_server via REST bridge.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, js_test_empty_file) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("js_test_empty_file.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  mysqlrouter::MySQLSession client;

  // connection should succeed on TCP, but fail to get the query
  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_THROW(
      client.connect("127.0.0.1", server_port, "username", "password", "", ""), std::exception);
}

/**
 * ensure 'stmts' being empty triggers an assertions at query.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, js_test_stmts_is_empty) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("js_test_stmts_is_empty.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port, "username", "password", "", ""));

  // no handler, it should throw
  SCOPED_TRACE("// select @@port");
  ASSERT_THROW(client.query_one("select @@port"), std::exception);
}

/**
 * ensure 'stmts' can be function.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, js_test_stmts_is_function) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("js_test_stmts_is_function.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port, "username", "password", "", ""));

  SCOPED_TRACE("// ping");
  ASSERT_NO_THROW(client.execute("ping"));
}

/**
 * ensure 'stmts' can be generator.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, js_test_stmts_is_coroutine) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("js_test_stmts_is_coroutine.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port, "username", "password", "", ""));

  SCOPED_TRACE("// ping");
  ASSERT_NO_THROW(client.execute("ping"));
}

/**
 * ensure 'stmts' can't be string.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, js_test_stmts_is_string) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("js_test_stmts_is_string.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_THROW(
      client.connect("127.0.0.1", server_port, "username", "password", "", ""), mysqlrouter::MySQLSession::Error);
}

/**
 * ensure 'stmts' is array works.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, js_test_stmts_is_array) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("js_test_stmts_is_array.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port, "username", "password", "", ""));

  SCOPED_TRACE("// ping");
  ASSERT_NO_THROW(client.execute("ping"));
}


/**
 * ensure 'metadata_3_secondaries.js' works.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerTest, metadata_3_secondaries_js) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("metadata_3_secondaries.js").str();
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000)) << server_mock.get_full_output();

  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port, "username", "password", "", ""));

  SCOPED_TRACE("// select @@port");
  ASSERT_NO_THROW(client.execute("select @@port"));
}



static void init_DIM() {
  mysql_harness::DIM& dim = mysql_harness::DIM::instance();

  // logging facility
  dim.set_LoggingRegistry(
    []() {
      static mysql_harness::logging::Registry registry;
      return &registry;
    },
    [](mysql_harness::logging::Registry*){}  // don't delete our static!
  );
  mysql_harness::logging::Registry& registry = dim.get_LoggingRegistry();

  mysql_harness::logging::g_HACK_default_log_level = "warning";
  mysql_harness::Config config;
  mysql_harness::logging::init_loggers(registry, config,
      {mysql_harness::logging::kMainLogger, "sql"},
      mysql_harness::logging::kMainLogger);
  mysql_harness::logging::create_main_logfile_handler(registry, "", "", true);

  registry.set_ready();
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  init_DIM();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

