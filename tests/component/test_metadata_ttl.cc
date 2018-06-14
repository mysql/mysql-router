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

#include "gmock/gmock.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"
#include "rapidjson/document.h"
#include "mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "keyring/keyring_manager.h"

#include <chrono>
#include <thread>

Path g_origin_path;
using mysqlrouter::MySQLSession;
using ::testing::PrintToString;

const std::string kMockServerGlobalsRestUri = "/api/v1/mock_server/globals/";

class MetadataChacheTTLTest : public RouterComponentTest {
 protected:
  virtual void SetUp() {
    set_origin(g_origin_path);
    RouterComponentTest::SetUp();
  }

  std::string get_metadata_cache_section(unsigned metadata_server_port, const std::string& ttl= "0.5") {
    return "[metadata_cache:test]\n"
           "router_id=1\n"
           "bootstrap_server_addresses=mysql://localhost:" + std::to_string(metadata_server_port) + "\n" +
           "user=mysql_router1_user\n"
           "metadata_cluster=test\n" +
           (ttl.empty() ? "" : std::string("ttl=" + ttl + "\n")) +
           "\n";
  }

  std::string get_metadata_cache_routing_section(unsigned router_port,
                                                 const std::string& role,
                                                 const std::string& strategy,
                                                 const std::string& mode = "") {
    std::string result = "[routing:test_default]\n"
      "bind_port=" + std::to_string(router_port) + "\n" +
      "destinations=metadata-cache://test/default?role=" + role + "\n" +
      "protocol=classic\n";

    if (!strategy.empty()) result += std::string("routing_strategy=" + strategy + "\n");
    if (!mode.empty()) result += std::string("mode=" + mode + "\n");

    return result;
  }

  std::string get_server_mock_globals_as_json_string(const unsigned http_port) {
    IOContext io_ctx;
    auto req = RestClient(io_ctx, "127.0.0.1", http_port).
      request_sync(HttpMethod::Get, kMockServerGlobalsRestUri);
    EXPECT_TRUE(req);
    EXPECT_EQ(req.get_response_code(), 200u);
    EXPECT_THAT(req.get_input_headers().get("Content-Type"), ::testing::StrEq("application/json"));
    auto resp_body = req.get_input_buffer();
    EXPECT_GT(resp_body.length(), 0u);
    auto resp_body_content = resp_body.pop_front(resp_body.length());

    // parse json
    std::string json_payload(resp_body_content.begin(), resp_body_content.end());
    return json_payload;
  }

  int get_ttl_queries_count(const std::string& json_string) {
    rapidjson::Document json_doc;
    json_doc.Parse(json_string.c_str());
    EXPECT_TRUE(json_doc.HasMember("md_query_count"));
    EXPECT_TRUE(json_doc["md_query_count"].IsInt());

    return json_doc["md_query_count"].GetInt();
  }

  RouterComponentTest::CommandHandle
  launch_router(unsigned router_port,
                const std::string& temp_test_dir,
                const std::string& metadata_cache_section,
                const std::string& routing_section,
                bool catch_stderr = true,
                bool with_sudo = false,
                bool wait_ready = true,
                bool log_to_stdout = false) {
    const std::string masterkey_file = Path(temp_test_dir).join("master.key").str();
    const std::string keyring_file = Path(temp_test_dir).join("keyring").str();
    mysql_harness::init_keyring(keyring_file, masterkey_file, true);
    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
    keyring->store("mysql_router1_user", "password", "root");
    mysql_harness::flush_keyring();
    mysql_harness::reset_keyring();

    // enable debug logs for better diagnostics in case of failure
    std::string logger_section = "[logger]\nlevel = DEBUG\n";

    // launch the router with metadata-cache configuration
    auto default_section = get_DEFAULT_defaults();
    default_section["keyring_path"] = keyring_file;
    default_section["master_key_path"] = masterkey_file;
    if (log_to_stdout) {
      default_section["logging_folder"] = "";
    }
    const std::string conf_file = create_config_file(logger_section + metadata_cache_section + routing_section, &default_section);
    auto router = RouterComponentTest::launch_router("-c " +  conf_file, catch_stderr, with_sudo);
    if (wait_ready) {
      bool ready = wait_for_port_ready(router_port, 1000);
      EXPECT_TRUE(ready) << get_router_log_output();
    }

    return router;
  }

  TcpPortPool port_pool_;
};

struct MetadataTTLTestParams {
  //  ttl value we want to set (floating point decimal in seconds)
  std::string ttl;
  // how long do we run the router and count the metadata queries
  std::chrono::milliseconds router_uptime;
  // how many metadata queries we expect over this period
  int expected_md_queries_count;
  // if true expected_md_queries_count is only a minimal expected
  // value, we should not check for maximum
  bool at_least;

  MetadataTTLTestParams(std::string ttl_,
                        std::chrono::milliseconds router_uptime_ = std::chrono::milliseconds(0),
                        int expected_md_queries_count_ = 0, bool at_least_ = false):
    ttl(ttl_), router_uptime(router_uptime_),
    expected_md_queries_count(expected_md_queries_count_), at_least(at_least_) {
  }
};

class MetadataChacheTTLTestParam : public MetadataChacheTTLTest,
                                   public ::testing::TestWithParam<MetadataTTLTestParams> {
protected:
 virtual void SetUp() {
   MetadataChacheTTLTest::SetUp();
 }
};

MATCHER_P2(IsBetween, a, b,
           std::string(negation ? "isn't" : "is") + " between " + PrintToString(a)
           + " and " + PrintToString(b)) {
  return a <= arg && arg <= b;
}

TEST_P(MetadataChacheTTLTestParam, CheckTTLValid) {

  auto test_params = GetParam();

  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr, [&](void*){purge_dir(temp_test_dir);});

  // launch the serevr mock (it's our metadata server and single cluster node)
  auto md_server_port =  port_pool_.get_next_available();
  auto md_server_http_port =  port_pool_.get_next_available();
  const std::string json_metadata = get_data_dir().join("metadata_1_node_repeat.js").str();

  auto metadata_server = launch_mysql_server_mock(json_metadata, md_server_port, false, md_server_http_port);
  bool ready = wait_for_port_ready(md_server_port, 1000);
  EXPECT_TRUE(ready) << metadata_server.get_full_output();


  // launch the router with metadata-cache configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section = get_metadata_cache_section(md_server_port, test_params.ttl);
  const std::string routing_section = get_metadata_cache_routing_section(router_port, "PRIMARY",
                                                                         "first-available");
  // on pb2 for Windows waiting for port being ready takes too long and this affects the number of
  // ttl queries during our sleep, on the other hand not waiting for it on some slow VMs (like macOS)
  // has the opposite effect
#ifdef _WIN32
  bool wait_port_ready = false;
#else
  bool wait_port_ready = true;
#endif
  auto router = launch_router(router_port, temp_test_dir, metadata_cache_section, routing_section, true, false,
    wait_port_ready);

  // keep the router running to see how many times it queries for metadata
  std::this_thread::sleep_for(test_params.router_uptime);

    // let's ask the mock how many metadata queries it got after
  std::string server_globals = get_server_mock_globals_as_json_string(md_server_http_port);
  int ttl_count = get_ttl_queries_count(server_globals);

  if (!test_params.at_least) {
    // it is timing based test so to decrease random failures chances let's
    // take some error marigin, we verify that number of metadata queries
    // falls into <expected_count-1, expected_count+1>
    EXPECT_THAT(ttl_count, IsBetween(test_params.expected_md_queries_count-1,
      test_params.expected_md_queries_count+1)) << get_router_log_output();
  }
  else {
    // we only check that the TTL was queried at least N times
    EXPECT_GE(ttl_count, test_params.expected_md_queries_count);
  }

  ASSERT_THAT(router.kill(), testing::Eq(0));
}


// Note: +1 becuase the router queries for the metadata twice when it initializes.
// Whenever that gets fixed and this test starts failing try removing '+1'
INSTANTIATE_TEST_CASE_P(CheckTTLIsUsedCorrectly,
                        MetadataChacheTTLTestParam,
                        ::testing::Values(
                           MetadataTTLTestParams("0.4", std::chrono::milliseconds(800), 2+1),
                           MetadataTTLTestParams("1", std::chrono::milliseconds(3000), 3+1),
                           // check that default is 0.5 if not provided:
                           MetadataTTLTestParams("", std::chrono::milliseconds(2000), 4+1),
                           // check that for 0 there are multiple ttl queries (we can't really guess
                           // how many there will be, but we should be able to safely assume that in 1 second
                           // it shold be at least 5 queries)
                           MetadataTTLTestParams("0", std::chrono::milliseconds(1000), 5+1,
                                                 /*at_least=*/ true)
                          )
                        );

class MetadataChacheTTLTestParamInvalid : public MetadataChacheTTLTest,
                                   public ::testing::TestWithParam<MetadataTTLTestParams> {
protected:
 virtual void SetUp() {
   MetadataChacheTTLTest::SetUp();
 }
};

TEST_P(MetadataChacheTTLTestParamInvalid, CheckTTLInvalid) {

  auto test_params = GetParam();

  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr, [&](void*){purge_dir(temp_test_dir);});

  // launch the serevr mock (it's our metadata server and single cluster node)
  auto md_server_port =  port_pool_.get_next_available();
  auto md_server_http_port =  port_pool_.get_next_available();
  const std::string json_metadata = get_data_dir().join("metadata_1_node_repeat.js").str();

  auto metadata_server = launch_mysql_server_mock(json_metadata, md_server_port, false, md_server_http_port);
  bool ready = wait_for_port_ready(md_server_port, 1000);
  EXPECT_TRUE(ready) << metadata_server.get_full_output();


  // launch the router with metadata-cache configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section = get_metadata_cache_section(md_server_port, test_params.ttl);
  const std::string routing_section = get_metadata_cache_routing_section(router_port, "PRIMARY",
                                                                         "first-available");
  auto router = launch_router(router_port, temp_test_dir, metadata_cache_section, routing_section,
                              true, false, false, /* log_to_stdout= */ true);

  EXPECT_EQ(router.wait_for_exit(), 1);
  EXPECT_THAT(router.exit_code(), testing::Ne(0));
  EXPECT_TRUE(router.expect_output("Configuration error: option ttl in [metadata_cache:test] needs value between 0 and 3600 inclusive"));
}

INSTANTIATE_TEST_CASE_P(CheckInvalidTTLRefusesStart,
                        MetadataChacheTTLTestParamInvalid,
                        ::testing::Values(
                           MetadataTTLTestParams("-0.001"),
                           MetadataTTLTestParams("3600.001"),
                           MetadataTTLTestParams("INVALID"),
                           MetadataTTLTestParams("1,1")
                          )
                        );

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
