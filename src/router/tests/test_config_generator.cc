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

// must be the first header, don't move it
#include <gtest/gtest_prod.h>

#include "cluster_metadata.h"
#include "config_generator.h"
#include "config_parser.h"
#include "dim.h"
#include "gtest_consoleoutput.h"
#include "mysql_session_replayer.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/utils.h"
#include "random_generator.h"
#include "router_app.h"
#include "router_test_helpers.h"
#include "mysqlrouter/uri.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <streambuf>
#include "keyring/keyring_manager.h"
#ifndef _WIN32
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

//ignore GMock warnings
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif

#include "gmock/gmock.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <mysql.h>

std::string g_cwd;
mysql_harness::Path g_origin;

class ConfigGeneratorTest : public ConsoleOutputTest {
protected:
  virtual void SetUp() {
    mysql_harness::DIM::instance().set_RandomGenerator(
      [](){ static mysql_harness::FakeRandomGenerator rg; return &rg; },
      [](mysql_harness::RandomGeneratorInterface*){}  // don't delete our static!
    );
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(g_cwd));
    config_path->append("Bug24570426.conf");

    default_paths["logging_folder"] = "";
  }

  std::unique_ptr<Path> config_path;
  std::map<std::string, std::string> default_paths;
};

using ::testing::Return;
using namespace testing;
using mysqlrouter::ConfigGenerator;


static void common_pass_metadata_checks(MySQLSessionReplayer &m) {
  m.expect_query_one("SELECT * FROM mysql_innodb_cluster_metadata.schema_version");
  m.then_return(2, {
      // major, minor
      {m.string_or_null("1"), m.string_or_null("0")}
    });

  m.expect_query_one("SELECT  ((SELECT count(*) FROM mysql_innodb_cluster_metadata.clusters) <= 1  AND (SELECT count(*) FROM mysql_innodb_cluster_metadata.replicasets) <= 1) as has_one_replicaset, (SELECT attributes->>'$.group_replication_group_name' FROM mysql_innodb_cluster_metadata.replicasets)  = @@group_replication_group_name as replicaset_is_ours");
  m.then_return(2, {
      // has_one_replicaset, replicaset_is_ours
      {m.string_or_null("1"), m.string_or_null()}
    });

  m.expect_query_one("SELECT member_state FROM performance_schema.replication_group_members WHERE member_id = @@server_uuid");
  m.then_return(1, {
      // member_state
      {m.string_or_null("ONLINE")}
    });

  m.expect_query_one("SELECT SUM(IF(member_state = 'ONLINE', 1, 0)) as num_onlines, COUNT(*) as num_total FROM performance_schema.replication_group_members");
  m.then_return(2, {
      // num_onlines, num_total
      {m.string_or_null("3"), m.string_or_null("3")}
    });

  m.expect_query_one("SELECT @@group_replication_single_primary_mode=1 as single_primary_mode,        (SELECT variable_value FROM performance_schema.global_status WHERE variable_name='group_replication_primary_member') as primary_member,         @@server_uuid as my_uuid");
  m.then_return(3, {
      // single_primary_mode, primary_member, my_uuid
      {m.string_or_null("0"), m.string_or_null("2d52f178-98f4-11e6-b0ff-8cc844fc24bf"), m.string_or_null("2d52f178-98f4-11e6-b0ff-8cc844fc24bf")}
    });
}

void set_mock_mysql(MySQLSessionReplayer* ptr) {
  mysql_harness::DIM::instance().set_MySQLSession(
    [ptr](){ return ptr; },
    [](mysqlrouter::MySQLSession*){}
  );
}

class ReplayerWithMockSSL : public MySQLSessionReplayer {
 public:
  void set_ssl_options(mysql_ssl_mode ssl_mode,
                       const std::string &tls_version,
                       const std::string &ssl_cipher,
                       const std::string &ca, const std::string &capath,
                       const std::string &crl, const std::string &crlpath) {
    last_ssl_mode = ssl_mode;
    last_tls_version = tls_version;
    last_ssl_cipher = ssl_cipher;
    last_ssl_ca = ca;
    last_ssl_capath = capath;
    last_ssl_crl = crl;
    last_ssl_crlpath = crlpath;
    if (should_throw_)
      throw Error("", 0);
  }

  void set_ssl_cert(const std::string &cert, const std::string &key) {
    last_ssl_cert = cert;
    last_ssl_key = key;
    if (should_throw_)
      throw Error("", 0);
  }

  void set_ssl_mode_should_fail(bool flag) { should_throw_ = flag; }

public:
  mysql_ssl_mode last_ssl_mode;
  std::string last_tls_version;
  std::string last_ssl_cipher;
  std::string last_ssl_ca;
  std::string last_ssl_capath;
  std::string last_ssl_crl;
  std::string last_ssl_crlpath;
  std::string last_ssl_cert;
  std::string last_ssl_key;

 private:
  bool should_throw_ = false;
};

TEST_F(ConfigGeneratorTest, fetch_bootstrap_servers_one) {
  MySQLSessionReplayer mock_mysql;

  std::string primary_cluster_name_;
  std::string primary_replicaset_servers_;
  std::string primary_replicaset_name_;
  bool multi_master_ = false;

  {
    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql);
    config_gen.init(&mock_mysql);

    mock_mysql.expect_query("").then_return(4, {
        {"mycluster", "myreplicaset", "pm", "somehost:3306"}});

    config_gen.fetch_bootstrap_servers(
      primary_replicaset_servers_,
      primary_cluster_name_, primary_replicaset_name_, multi_master_);

    ASSERT_THAT(primary_replicaset_servers_, Eq("mysql://somehost:3306"));
    ASSERT_THAT(primary_cluster_name_, Eq("mycluster"));
    ASSERT_THAT(primary_replicaset_name_, Eq("myreplicaset"));
    ASSERT_THAT(multi_master_, Eq(false));
  }

  {
    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql);
    config_gen.init(&mock_mysql);

    mock_mysql.expect_query("").then_return(4, {
        {"mycluster", "myreplicaset", "mm", "somehost:3306"}});

    config_gen.fetch_bootstrap_servers(
      primary_replicaset_servers_,
      primary_cluster_name_, primary_replicaset_name_, multi_master_);

    ASSERT_THAT(primary_replicaset_servers_, Eq("mysql://somehost:3306"));
    ASSERT_THAT(primary_cluster_name_, Eq("mycluster"));
    ASSERT_THAT(primary_replicaset_name_, Eq("myreplicaset"));
    ASSERT_THAT(multi_master_, Eq(true));
  }

  {
    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql);
    config_gen.init(&mock_mysql);

    mock_mysql.expect_query("").then_return(4, {
        {"mycluster", "myreplicaset", "xxx", "somehost:3306"}});

    ASSERT_THROW(
      config_gen.fetch_bootstrap_servers(
        primary_replicaset_servers_,
        primary_cluster_name_, primary_replicaset_name_, multi_master_),
      std::runtime_error);
  }
}

TEST_F(ConfigGeneratorTest, fetch_bootstrap_servers_three) {
  MySQLSessionReplayer mock_mysql;

  std::string primary_cluster_name_;
  std::string primary_replicaset_servers_;
  std::string primary_replicaset_name_;
  bool multi_master_ = false;

  {
    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql);
    config_gen.init(&mock_mysql);

    // "F.cluster_name, "
    // "R.replicaset_name, "
    // "R.topology_type, "
    // "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic')) "
    mock_mysql.expect_query("").then_return(4, {
        {"mycluster", "myreplicaset", "pm", "somehost:3306"},
        {"mycluster", "myreplicaset", "pm", "otherhost:3306"},
        {"mycluster", "myreplicaset", "pm", "sumhost:3306"}});

    config_gen.fetch_bootstrap_servers(
      primary_replicaset_servers_,
      primary_cluster_name_, primary_replicaset_name_, multi_master_);

    ASSERT_THAT(primary_replicaset_servers_, Eq("mysql://somehost:3306,mysql://otherhost:3306,mysql://sumhost:3306"));
    ASSERT_THAT(primary_cluster_name_, Eq("mycluster"));
    ASSERT_THAT(primary_replicaset_name_, Eq("myreplicaset"));
    ASSERT_THAT(multi_master_, Eq(false));
  }
}

TEST_F(ConfigGeneratorTest, fetch_bootstrap_servers_multiple_replicasets) {
  MySQLSessionReplayer mock_mysql;

  std::string primary_cluster_name_;
  std::string primary_replicaset_servers_;
  std::string primary_replicaset_name_;
  bool multi_master_ = false;

  {
    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql);
    config_gen.init(&mock_mysql);
    mock_mysql.expect_query("").then_return(4, {
        {"mycluster", "myreplicaset", "pm", "somehost:3306"},
        {"mycluster", "anotherreplicaset", "pm", "otherhost:3306"}});

    ASSERT_THROW(
      config_gen.fetch_bootstrap_servers(
        primary_replicaset_servers_,
        primary_cluster_name_, primary_replicaset_name_, multi_master_),
      std::runtime_error);
  }

  {
    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql);
    config_gen.init(&mock_mysql);
    mock_mysql.expect_query("").then_return(4, {
        {"mycluster", "myreplicaset", "pm", "somehost:3306"},
        {"anothercluster", "anotherreplicaset", "pm", "otherhost:3306"}});

    ASSERT_THROW(
      config_gen.fetch_bootstrap_servers(
        primary_replicaset_servers_,
        primary_cluster_name_, primary_replicaset_name_, multi_master_),
      std::runtime_error);
  }
}


TEST_F(ConfigGeneratorTest, fetch_bootstrap_servers_invalid) {
  MySQLSessionReplayer mock_mysql;

  std::string primary_cluster_name_;
  std::string primary_replicaset_servers_;
  std::string primary_replicaset_name_;
  bool multi_master_ = false;

  {
    ConfigGenerator config_gen;
    common_pass_metadata_checks(mock_mysql);
    config_gen.init(&mock_mysql);

    mock_mysql.expect_query("").then_return(4, {});
    // no replicasets/clusters defined
    ASSERT_THROW(
      config_gen.fetch_bootstrap_servers(
        primary_replicaset_servers_,
        primary_cluster_name_, primary_replicaset_name_, multi_master_),
      std::runtime_error
    );
  }
}

TEST_F(ConfigGeneratorTest, create_acount) {
  // using password directly
  {
    MySQLSessionReplayer mock_mysql;

    ::testing::InSequence s;
    common_pass_metadata_checks(mock_mysql);
    mock_mysql.expect_execute("DROP USER IF EXISTS cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("CREATE USER cluster_user@'%' IDENTIFIED BY 'secret'").then_ok();
    mock_mysql.expect_execute("GRANT SELECT ON mysql_innodb_cluster_metadata.* TO cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("GRANT SELECT ON performance_schema.replication_group_members TO cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("GRANT SELECT ON performance_schema.replication_group_member_stats TO cluster_user@'%'").then_ok();

    ConfigGenerator config_gen;
    config_gen.init(&mock_mysql);
    config_gen.create_account("cluster_user", "secret");
  }
  // using hashed password
  {
    MySQLSessionReplayer mock_mysql;

    ::testing::InSequence s;
    common_pass_metadata_checks(mock_mysql);
    mock_mysql.expect_execute("DROP USER IF EXISTS cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("CREATE USER cluster_user@'%' IDENTIFIED WITH mysql_native_password "
                              "AS '*89C1E57BE94931A2C11EB6C76E4C254799853B8D'").then_ok();
    mock_mysql.expect_execute("GRANT SELECT ON mysql_innodb_cluster_metadata.* TO cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("GRANT SELECT ON performance_schema.replication_group_members TO cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("GRANT SELECT ON performance_schema.replication_group_member_stats TO cluster_user@'%'").then_ok();

    ConfigGenerator config_gen;
    config_gen.init(&mock_mysql);
    config_gen.create_account("cluster_user", "*89C1E57BE94931A2C11EB6C76E4C254799853B8D", true);
  }
}


TEST_F(ConfigGeneratorTest, create_config_single_master) {
  StrictMock<MySQLSessionReplayer> mock_mysql;

  std::map<std::string, std::string> user_options;

  ConfigGenerator config_gen;
  common_pass_metadata_checks(mock_mysql);
  config_gen.init(&mock_mysql);
  ConfigGenerator::Options options = config_gen.fill_options(false, user_options);

  {
    std::stringstream output;
    config_gen.create_config(output,
                        123, "myrouter", "mysqlrouter",
                        "server1,server2,server3",
                        "mycluster",
                        "myreplicaset",
                        "cluster_user",
                        options);
    ASSERT_THAT(output.str(),
      Eq("# File automatically generated during MySQL Router bootstrap\n"
        "[DEFAULT]\n"
        "name=myrouter\n"
        "user=mysqlrouter\n"
        "\n"
        "[logger]\n"
        "level = INFO\n"
        "\n"
        "[metadata_cache:mycluster]\n"
        "router_id=123\n"
        "bootstrap_server_addresses=server1,server2,server3\n"
        "user=cluster_user\n"
        "metadata_cluster=mycluster\n"
        "ttl=300\n"
        "\n"
        "[routing:mycluster_myreplicaset_rw]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=6446\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "protocol=classic\n"
        "\n"
        "[routing:mycluster_myreplicaset_ro]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=6447\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=SECONDARY\n"
        "mode=read-only\n"
        "protocol=classic\n"
        "\n"
        "[routing:mycluster_myreplicaset_x_rw]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=64460\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "protocol=x\n"
        "\n"
        "[routing:mycluster_myreplicaset_x_ro]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=64470\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=SECONDARY\n"
        "mode=read-only\n"
        "protocol=x\n"
        "\n"));
  }
  {
    std::stringstream output;
    // system instance (no key)
    config_gen.create_config(output,
                        123, "", "",
                        "server1,server2,server3",
                        "mycluster",
                        "myreplicaset",
                        "cluster_user",
                        options);
    ASSERT_THAT(output.str(),
      Eq("# File automatically generated during MySQL Router bootstrap\n"
          "[DEFAULT]\n"
          "\n"
          "[logger]\n"
          "level = INFO\n"
          "\n"
          "[metadata_cache:mycluster]\n"
          "router_id=123\n"
          "bootstrap_server_addresses=server1,server2,server3\n"
          "user=cluster_user\n"
          "metadata_cluster=mycluster\n"
          "ttl=300\n"
          "\n"
          "[routing:mycluster_myreplicaset_rw]\n"
          "bind_address=0.0.0.0\n"
          "bind_port=6446\n"
          "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
          "mode=read-write\n"
          "protocol=classic\n"
          "\n"
          "[routing:mycluster_myreplicaset_ro]\n"
          "bind_address=0.0.0.0\n"
          "bind_port=6447\n"
          "destinations=metadata-cache://mycluster/myreplicaset?role=SECONDARY\n"
          "mode=read-only\n"
          "protocol=classic\n"
          "\n"
          "[routing:mycluster_myreplicaset_x_rw]\n"
          "bind_address=0.0.0.0\n"
          "bind_port=64460\n"
          "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
          "mode=read-write\n"
          "protocol=x\n"
          "\n"
          "[routing:mycluster_myreplicaset_x_ro]\n"
          "bind_address=0.0.0.0\n"
          "bind_port=64470\n"
          "destinations=metadata-cache://mycluster/myreplicaset?role=SECONDARY\n"
          "mode=read-only\n"
          "protocol=x\n"
          "\n"));
  }
  {
    std::stringstream output;
    auto opts = user_options;
    opts["base-port"] = "1234";
    options = config_gen.fill_options(false, opts);

    config_gen.create_config(output,
                        123, "", "",
                        "server1,server2,server3",
                        "mycluster",
                        "myreplicaset",
                        "cluster_user",
                        options);
    ASSERT_THAT(output.str(),
      Eq("# File automatically generated during MySQL Router bootstrap\n"
        "[DEFAULT]\n"
        "\n"
        "[logger]\n"
        "level = INFO\n"
        "\n"
        "[metadata_cache:mycluster]\n"
        "router_id=123\n"
        "bootstrap_server_addresses=server1,server2,server3\n"
        "user=cluster_user\n"
        "metadata_cluster=mycluster\n"
        "ttl=300\n"
        "\n"
        "[routing:mycluster_myreplicaset_rw]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=1234\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "protocol=classic\n"
        "\n"
        "[routing:mycluster_myreplicaset_ro]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=1235\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=SECONDARY\n"
        "mode=read-only\n"
        "protocol=classic\n"
        "\n"
        "[routing:mycluster_myreplicaset_x_rw]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=1236\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "protocol=x\n"
        "\n"
        "[routing:mycluster_myreplicaset_x_ro]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=1237\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=SECONDARY\n"
        "mode=read-only\n"
        "protocol=x\n"
        "\n"));
  }
  {
    std::stringstream output;
    auto opts = user_options;
    opts["base-port"] = "123";
    opts["use-sockets"] = "1";
    opts["skip-tcp"] = "1";
    opts["socketsdir"] = "/tmp";
    options = config_gen.fill_options(false, opts);

    config_gen.create_config(output,
                        123, "", "",
                        "server1,server2,server3",
                        "mycluster",
                        "myreplicaset",
                        "cluster_user",
                        options);
    ASSERT_THAT(output.str(),
      Eq("# File automatically generated during MySQL Router bootstrap\n"
        "[DEFAULT]\n"
        "\n"
        "[logger]\n"
        "level = INFO\n"
        "\n"
        "[metadata_cache:mycluster]\n"
        "router_id=123\n"
        "bootstrap_server_addresses=server1,server2,server3\n"
        "user=cluster_user\n"
        "metadata_cluster=mycluster\n"
        "ttl=300\n"
        "\n"
        "[routing:mycluster_myreplicaset_rw]\n"
        "socket=/tmp/mysql.sock\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "protocol=classic\n"
        "\n"
        "[routing:mycluster_myreplicaset_ro]\n"
        "socket=/tmp/mysqlro.sock\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=SECONDARY\n"
        "mode=read-only\n"
        "protocol=classic\n"
        "\n"
        "[routing:mycluster_myreplicaset_x_rw]\n"
        "socket=/tmp/mysqlx.sock\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "protocol=x\n"
        "\n"
        "[routing:mycluster_myreplicaset_x_ro]\n"
        "socket=/tmp/mysqlxro.sock\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=SECONDARY\n"
        "mode=read-only\n"
        "protocol=x\n"
        "\n"));
  }
  {
    std::stringstream output;
    auto opts = user_options;
    opts["use-sockets"] = "1";
    opts["socketsdir"] = "/tmp";
    options = config_gen.fill_options(false, opts);

    config_gen.create_config(output,
                        123, "", "",
                        "server1,server2,server3",
                        "mycluster",
                        "myreplicaset",
                        "cluster_user",
                        options);
    ASSERT_THAT(output.str(),
      Eq("# File automatically generated during MySQL Router bootstrap\n"
        "[DEFAULT]\n"
        "\n"
        "[logger]\n"
        "level = INFO\n"
        "\n"
        "[metadata_cache:mycluster]\n"
        "router_id=123\n"
        "bootstrap_server_addresses=server1,server2,server3\n"
        "user=cluster_user\n"
        "metadata_cluster=mycluster\n"
        "ttl=300\n"
        "\n"
        "[routing:mycluster_myreplicaset_rw]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=6446\n"
        "socket=/tmp/mysql.sock\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "protocol=classic\n"
        "\n"
        "[routing:mycluster_myreplicaset_ro]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=6447\n"
        "socket=/tmp/mysqlro.sock\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=SECONDARY\n"
        "mode=read-only\n"
        "protocol=classic\n"
        "\n"
        "[routing:mycluster_myreplicaset_x_rw]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=64460\n"
        "socket=/tmp/mysqlx.sock\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "protocol=x\n"
        "\n"
        "[routing:mycluster_myreplicaset_x_ro]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=64470\n"
        "socket=/tmp/mysqlxro.sock\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=SECONDARY\n"
        "mode=read-only\n"
        "protocol=x\n"
        "\n"));
  }
  {
    std::stringstream output;
    auto opts = user_options;
    opts["bind-address"] = "127.0.0.1";
    options = config_gen.fill_options(false, opts);

    config_gen.create_config(output,
                        123, "myrouter", "mysqlrouter",
                        "server1,server2,server3",
                        "mycluster",
                        "myreplicaset",
                        "cluster_user",
                        options);
    ASSERT_THAT(output.str(),
      Eq("# File automatically generated during MySQL Router bootstrap\n"
        "[DEFAULT]\n"
        "name=myrouter\n"
        "user=mysqlrouter\n"
        "\n"
        "[logger]\n"
        "level = INFO\n"
        "\n"
        "[metadata_cache:mycluster]\n"
        "router_id=123\n"
        "bootstrap_server_addresses=server1,server2,server3\n"
        "user=cluster_user\n"
        "metadata_cluster=mycluster\n"
        "ttl=300\n"
        "\n"
        "[routing:mycluster_myreplicaset_rw]\n"
        "bind_address=127.0.0.1\n"
        "bind_port=6446\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "protocol=classic\n"
        "\n"
        "[routing:mycluster_myreplicaset_ro]\n"
        "bind_address=127.0.0.1\n"
        "bind_port=6447\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=SECONDARY\n"
        "mode=read-only\n"
        "protocol=classic\n"
        "\n"
        "[routing:mycluster_myreplicaset_x_rw]\n"
        "bind_address=127.0.0.1\n"
        "bind_port=64460\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "protocol=x\n"
        "\n"
        "[routing:mycluster_myreplicaset_x_ro]\n"
        "bind_address=127.0.0.1\n"
        "bind_port=64470\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=SECONDARY\n"
        "mode=read-only\n"
        "protocol=x\n"
        "\n"));
  }

}


TEST_F(ConfigGeneratorTest, create_config_multi_master) {
  std::stringstream output;
  StrictMock<MySQLSessionReplayer> mock_mysql;

  std::map<std::string, std::string> user_options;

  ConfigGenerator config_gen;
  common_pass_metadata_checks(mock_mysql);
  config_gen.init(&mock_mysql);
  ConfigGenerator::Options options = config_gen.fill_options(true, user_options);
  config_gen.create_config(output,
                      123, "myrouter", "",
                      "server1,server2,server3",
                      "mycluster",
                      "myreplicaset",
                      "cluster_user",
                      options);
  ASSERT_THAT(output.str(),
    Eq("# File automatically generated during MySQL Router bootstrap\n"
        "[DEFAULT]\n"
        "name=myrouter\n"
        "\n"
        "[logger]\n"
        "level = INFO\n"
        "\n"
        "[metadata_cache:mycluster]\n"
        "router_id=123\n"
        "bootstrap_server_addresses=server1,server2,server3\n"
        "user=cluster_user\n"
        "metadata_cluster=mycluster\n"
        "ttl=300\n"
        "\n"
        "[routing:mycluster_myreplicaset_rw]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=6446\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "protocol=classic\n"
        "\n"
        "[routing:mycluster_myreplicaset_x_rw]\n"
        "bind_address=0.0.0.0\n"
        "bind_port=64460\n"
        "destinations=metadata-cache://mycluster/myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "protocol=x\n"
        "\n"));
}

TEST_F(ConfigGeneratorTest, fill_options) {
  StrictMock<MySQLSessionReplayer> mock_mysql;

  ConfigGenerator config_gen;
  common_pass_metadata_checks(mock_mysql);
  config_gen.init(&mock_mysql);

  ConfigGenerator::Options options;
  {
    std::map<std::string, std::string> user_options;
    options = config_gen.fill_options(true, user_options);
    ASSERT_THAT(options.multi_master, Eq(true));
    ASSERT_THAT(options.bind_address, Eq(""));
    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(6446));
    ASSERT_THAT(options.rw_endpoint.socket, Eq(""));
    ASSERT_THAT(options.ro_endpoint, Eq(false));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(false));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    user_options["bind-address"] = "127.0.0.1";
    options = config_gen.fill_options(true, user_options);
    ASSERT_THAT(options.multi_master, Eq(true));
    ASSERT_THAT(options.bind_address, Eq("127.0.0.1"));
    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(6446));
    ASSERT_THAT(options.rw_endpoint.socket, Eq(""));
    ASSERT_THAT(options.ro_endpoint, Eq(false));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(false));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    user_options["base-port"] = "1234";
    options = config_gen.fill_options(false, user_options);
    ASSERT_THAT(options.multi_master, Eq(false));
    ASSERT_THAT(options.bind_address, Eq(""));
    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(1234));
    ASSERT_THAT(options.rw_endpoint.socket, Eq(""));
    ASSERT_THAT(options.ro_endpoint, Eq(true));
    ASSERT_THAT(options.ro_endpoint.port, Eq(1235));
    ASSERT_THAT(options.ro_endpoint.socket, Eq(""));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(true));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    user_options["base-port"] = "1";
    options = config_gen.fill_options(false, user_options);
    ASSERT_THAT(options.rw_endpoint.port, Eq(1));
    user_options["base-port"] = "3306";
    options = config_gen.fill_options(false, user_options);
    ASSERT_THAT(options.rw_endpoint.port, Eq(3306));
    user_options["base-port"] = "";
    ASSERT_THROW(
      options = config_gen.fill_options(false, user_options),
      std::runtime_error);
    user_options["base-port"] = "-1";
    ASSERT_THROW(
      options = config_gen.fill_options(false, user_options),
      std::runtime_error);
    user_options["base-port"] = "999999";
    ASSERT_THROW(
      options = config_gen.fill_options(false, user_options),
      std::runtime_error);
    user_options["base-port"] = "0";
    ASSERT_THROW(
      options = config_gen.fill_options(false, user_options),
      std::runtime_error);
    user_options["base-port"] = "65536";
    ASSERT_THROW(
      options = config_gen.fill_options(false, user_options),
      std::runtime_error);
    user_options["base-port"] = "2000bozo";
    ASSERT_THROW(
      options = config_gen.fill_options(false, user_options),
      std::runtime_error);

    // Bug #24808309
    user_options["base-port"] = "65533";
    ASSERT_THROW_LIKE(
      options = config_gen.fill_options(false, user_options),
      std::runtime_error,
      "Invalid base-port number");

    user_options["base-port"] = "65532";
    ASSERT_NO_THROW(options = config_gen.fill_options(false, user_options));

    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(65532));
    ASSERT_THAT(options.rw_endpoint.socket, Eq(""));
    ASSERT_THAT(options.ro_endpoint, Eq(true));
    ASSERT_THAT(options.ro_endpoint.port, Eq(65533));
    ASSERT_THAT(options.ro_endpoint.socket, Eq(""));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(true));
    ASSERT_THAT(options.rw_x_endpoint.port, Eq(65534));
    ASSERT_THAT(options.rw_x_endpoint.socket, Eq(""));
    ASSERT_THAT(options.ro_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint.port, Eq(65535));
    ASSERT_THAT(options.ro_x_endpoint.socket, Eq(""));
  }
  {
     std::map<std::string, std::string> user_options;
     user_options["bind-address"] = "invalid";
     ASSERT_THROW(
       options = config_gen.fill_options(false, user_options),
       std::runtime_error);
     user_options["bind-address"] = "";
     ASSERT_THROW(
       options = config_gen.fill_options(false, user_options),
       std::runtime_error);
     user_options["bind-address"] = "1.2.3.4.5";
     ASSERT_THROW(
       options = config_gen.fill_options(false, user_options),
       std::runtime_error);
   }
  {
    std::map<std::string, std::string> user_options;
    user_options["use-sockets"] = "1";
    user_options["skip-tcp"] = "1";
    options = config_gen.fill_options(false, user_options);
    ASSERT_THAT(options.multi_master, Eq(false));
    ASSERT_THAT(options.bind_address, Eq(""));
    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(0));
    ASSERT_THAT(options.rw_endpoint.socket, Eq("mysql.sock"));
    ASSERT_THAT(options.ro_endpoint, Eq(true));
    ASSERT_THAT(options.ro_endpoint.port, Eq(0));
    ASSERT_THAT(options.ro_endpoint.socket, Eq("mysqlro.sock"));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(true));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    user_options["skip-tcp"] = "1";
    options = config_gen.fill_options(false, user_options);
    ASSERT_THAT(options.multi_master, Eq(false));
    ASSERT_THAT(options.bind_address, Eq(""));
    ASSERT_THAT(options.rw_endpoint, Eq(false));
    ASSERT_THAT(options.rw_endpoint.port, Eq(0));
    ASSERT_THAT(options.rw_endpoint.socket, Eq(""));
    ASSERT_THAT(options.ro_endpoint, Eq(false));
    ASSERT_THAT(options.ro_endpoint.port, Eq(0));
    ASSERT_THAT(options.ro_endpoint.socket, Eq(""));
    ASSERT_THAT(options.rw_x_endpoint, Eq(false));
    ASSERT_THAT(options.ro_x_endpoint, Eq(false));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    user_options["use-sockets"] = "1";
    options = config_gen.fill_options(false, user_options);
    ASSERT_THAT(options.multi_master, Eq(false));
    ASSERT_THAT(options.bind_address, Eq(""));
    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(6446));
    ASSERT_THAT(options.rw_endpoint.socket, Eq("mysql.sock"));
    ASSERT_THAT(options.ro_endpoint, Eq(true));
    ASSERT_THAT(options.ro_endpoint.port, Eq(6447));
    ASSERT_THAT(options.ro_endpoint.socket, Eq("mysqlro.sock"));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(true));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
  {
    std::map<std::string, std::string> user_options;
    options = config_gen.fill_options(false, user_options);
    ASSERT_THAT(options.multi_master, Eq(false));
    ASSERT_THAT(options.bind_address, Eq(""));
    ASSERT_THAT(options.rw_endpoint, Eq(true));
    ASSERT_THAT(options.rw_endpoint.port, Eq(6446));
    ASSERT_THAT(options.rw_endpoint.socket, Eq(""));
    ASSERT_THAT(options.ro_endpoint, Eq(true));
    ASSERT_THAT(options.ro_endpoint.port, Eq(6447));
    ASSERT_THAT(options.ro_endpoint.socket, Eq(""));
    ASSERT_THAT(options.rw_x_endpoint, Eq(true));
    ASSERT_THAT(options.ro_x_endpoint, Eq(true));
    ASSERT_THAT(options.override_logdir, Eq(""));
    ASSERT_THAT(options.override_rundir, Eq(""));
    ASSERT_THAT(options.override_datadir, Eq(""));
  }
}

//XXX TODO: add recursive directory delete function

namespace {
enum action_t{ACTION_EXECUTE, ACTION_QUERY, ACTION_ERROR};

struct query_entry_t{
  const char *query;
  action_t action;
  uint64_t last_insert_id;
  unsigned error_code;

  query_entry_t(const char *query_, action_t action_, uint64_t last_insert_id_ = 0, unsigned error_code_ = 0):
    query(query_), action(action_), last_insert_id(last_insert_id_), error_code(error_code_)
  {}
};

std::vector<query_entry_t> expected_bootstrap_queries = {
  {"START TRANSACTION", ACTION_EXECUTE},
  {"SELECT host_id, host_name", ACTION_QUERY},
  {"INSERT INTO mysql_innodb_cluster_metadata.hosts", ACTION_EXECUTE},
  {"INSERT INTO mysql_innodb_cluster_metadata.routers", ACTION_EXECUTE, 4},
  {"DROP USER IF EXISTS mysql_router4_012345678901@'%'", ACTION_EXECUTE},
  {"CREATE USER mysql_router4_012345678901@'%'", ACTION_EXECUTE},
  {"GRANT SELECT ON mysql_innodb_cluster_metadata.* TO mysql_router4_012345678901@'%'", ACTION_EXECUTE},
  {"GRANT SELECT ON performance_schema.replication_group_members TO mysql_router4_012345678901@'%'", ACTION_EXECUTE},
  {"GRANT SELECT ON performance_schema.replication_group_member_stats TO mysql_router4_012345678901@'%'", ACTION_EXECUTE},
  {"UPDATE mysql_innodb_cluster_metadata.routers SET attributes = ", ACTION_EXECUTE},
  {"COMMIT", ACTION_EXECUTE},
};

static void expect_bootstrap_queries(MySQLSessionReplayer &m, const char *cluster_name,
                                     const std::vector<query_entry_t> &expected_queries = expected_bootstrap_queries) {
  m.expect_query("").then_return(4, {{cluster_name, "myreplicaset", "pm", "somehost:3306"}});
  for (const auto &query: expected_queries) {
    switch (query.action) {
    case ACTION_EXECUTE:
      m.expect_execute(query.query).then_ok(query.last_insert_id);
      break;
    case ACTION_QUERY:
      m.expect_query_one(query.query).then_return(2, {});
      break;
    default: /*ACTION_ERROR*/
      m.expect_execute(query.query).then_error("ERROR:", query.error_code);
    }
  }
}

void bootstrap_name_test(const std::string &dir,
                         const std::string &name,
                         bool expect_fail,
                         const std::map<std::string, std::string> &default_paths) {
  StrictMock<MySQLSessionReplayer> mysql;
  ::testing::InSequence s;

  ConfigGenerator config_gen;
  common_pass_metadata_checks(mysql);
  config_gen.init(&mysql);
  if (!expect_fail)
    expect_bootstrap_queries(mysql, "mycluster");

  std::map<std::string, std::string> options;
  options["name"] = name;
  options["quiet"] = "1";
  options["id"] = "4";
  config_gen.bootstrap_directory_deployment(dir,
      options, default_paths, "delme", "delme.key");
}

} // anonymous namespace

TEST_F(ConfigGeneratorTest, bootstrap_invalid_name) {
  const std::string dir = "./bug24807941";
  mysqlrouter::delete_recursive(dir);

  // Bug#24807941
  ASSERT_NO_THROW(bootstrap_name_test(dir, "myname", false, default_paths));
  mysqlrouter::delete_recursive(dir);
  mysql_harness::reset_keyring();

  ASSERT_NO_THROW(bootstrap_name_test(dir, "myname", false, default_paths));
  mysqlrouter::delete_recursive(dir);
  mysql_harness::reset_keyring();

  ASSERT_NO_THROW(bootstrap_name_test(dir, "", false, default_paths));
  mysqlrouter::delete_recursive(dir);
  mysql_harness::reset_keyring();

  ASSERT_THROW_LIKE(
    bootstrap_name_test(dir, "system", true, default_paths),
    std::runtime_error,
    "Router name 'system' is reserved");
  mysqlrouter::delete_recursive(dir);
  mysql_harness::reset_keyring();

  std::vector<std::string> bad_names{
    "new\nline",
    "car\rreturn",
  };
  for (std::string &name : bad_names) {
    ASSERT_THROW_LIKE(
      bootstrap_name_test(dir, name, true, default_paths),
      std::runtime_error,
      "Router name '"+name+"' contains invalid characters.");
    mysqlrouter::delete_recursive(dir);
    mysql_harness::reset_keyring();
  }

  ASSERT_THROW_LIKE(
    bootstrap_name_test(dir, "veryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryverylongname", true, default_paths),
    std::runtime_error,
    "too long (max 255).");
  mysqlrouter::delete_recursive(dir);
  mysql_harness::reset_keyring();
}


TEST_F(ConfigGeneratorTest, bootstrap_cleanup_on_failure) {
  const std::string dir = "./bug24808634";
  mysqlrouter::delete_recursive(dir);
  mysqlrouter::delete_file("./bug24808634/delme.key");

  ASSERT_FALSE(mysql_harness::Path(dir).exists());
  ASSERT_FALSE(mysql_harness::Path("./bug24808634/delme.key").exists());
  // cleanup on failure when dir didn't exist before
  {
    MySQLSessionReplayer mysql;

    ConfigGenerator config_gen;
    common_pass_metadata_checks(mysql);
    config_gen.init(&mysql);
    mysql.expect_query("SELECT F.cluster_name").then_return(4, {{"mycluter", "myreplicaset", "pm", "somehost:3306"}});
    mysql.expect_execute("START TRANSACTION").then_error("boo!", 1234);

    std::map<std::string, std::string> options;
    options["name"] = "foobar";
    options["quiet"] = "1";
    ASSERT_THROW_LIKE(
      config_gen.bootstrap_directory_deployment(dir,
          options, default_paths, "delme", "delme.key"),
      mysqlrouter::MySQLSession::Error,
      "boo!");

    ASSERT_FALSE(mysql_harness::Path(dir).exists());
    ASSERT_FALSE(mysql_harness::Path("./bug24808634/delme.key").exists());
  }
  mysql_harness::reset_keyring();

  // this should succeed, so that we can test that cleanup doesn't delete existing stuff
  {
    MySQLSessionReplayer mysql;

    ConfigGenerator config_gen;
    common_pass_metadata_checks(mysql);
    config_gen.init(&mysql);
    expect_bootstrap_queries(mysql, "mycluster");

    std::map<std::string, std::string> options;
    options["name"] = "foobar";
    options["quiet"] = "1";
    ASSERT_NO_THROW(
      config_gen.bootstrap_directory_deployment(dir,
          options, default_paths, "delme", "delme.key"));

    ASSERT_TRUE(mysql_harness::Path(dir).exists());
    ASSERT_TRUE(mysql_harness::Path("./bug24808634/delme.key").exists());
  }
  mysql_harness::reset_keyring();

  // don't cleanup on failure if dir already existed before
  {
    MySQLSessionReplayer mysql;

    ConfigGenerator config_gen;
      common_pass_metadata_checks(mysql);
    config_gen.init(&mysql);
    mysql.expect_query("").then_return(4, {{"mycluster", "myreplicaset", "pm", "somehost:3306"}});
    // force a failure during account creationg
    mysql.expect_execute("").then_error("boo!", 1234);

    std::map<std::string, std::string> options;
    options["name"] = "foobar";
    options["quiet"] = "1";
    ASSERT_THROW_LIKE(
      config_gen.bootstrap_directory_deployment(dir,
            options, default_paths, "delme", "delme.key"),
      std::runtime_error,
      "boo!");

    ASSERT_TRUE(mysql_harness::Path(dir).exists());
    ASSERT_TRUE(mysql_harness::Path(dir).join("delme.key").exists());
  }
  mysql_harness::reset_keyring();

  // don't cleanup on failure in early validation if dir already existed before
  {
    MySQLSessionReplayer mysql;

    ConfigGenerator config_gen;
      common_pass_metadata_checks(mysql);
    config_gen.init(&mysql);
    mysql.expect_query("").then_return(4, {{"mycluter", "myreplicaset", "pm", "somehost:3306"}});

    std::map<std::string, std::string> options;
    options["name"] = "force\nfailure";
    options["quiet"] = "1";
    ASSERT_THROW(
      config_gen.bootstrap_directory_deployment(dir,
            options, default_paths, "delme", "delme.key"),
      std::runtime_error);
    ASSERT_TRUE(mysql_harness::Path(dir).exists());
    ASSERT_TRUE(mysql_harness::Path(dir).join("delme.key").exists());
  }
  mysql_harness::reset_keyring();
  mysqlrouter::delete_recursive(dir);
  mysqlrouter::delete_file("./bug24808634/delme.key");
}

TEST_F(ConfigGeneratorTest, bug25391460) {
  const std::string dir = "./bug25391460";
  mysqlrouter::delete_recursive(dir);

  // Bug#24807941
  {
    MySQLSessionReplayer mysql;

    ConfigGenerator config_gen;
    common_pass_metadata_checks(mysql);
    expect_bootstrap_queries(mysql, "mycluster");
    config_gen.init(&mysql);
    mysql.expect_query("").then_return(4, {{"mycluster", "myreplicaset", "pm", "somehost:3306"}});

    std::map<std::string, std::string> options;
    options["quiet"] = "1";
    options["use-sockets"] = "1";
    ASSERT_NO_THROW(
      config_gen.bootstrap_directory_deployment(dir,
            options, default_paths, "delme", "delme.key"));
    ASSERT_TRUE(mysql_harness::Path(dir).exists());
    ASSERT_TRUE(mysql_harness::Path(dir).join("delme.key").exists());
  }

  // now read the config file and check that all socket paths are
  // .../bug25391460/mysql*.sock instead of .../bug25391460/socketsdir/mysql*.sock
  std::ifstream cf;
  std::string basedir = mysql_harness::Path(dir).real_path().str();
  cf.open(mysql_harness::Path(dir).join("mysqlrouter.conf").str());
  while (!cf.eof()) {
    std::string line;
    cf >> line;
    if (line.compare(0, 7, "socket=") == 0) {
      line = line.substr(7);
      // check prefix/basedir
      EXPECT_EQ(basedir, line.substr(0, basedir.length()));
      std::string suffix = line.substr(basedir.length()+1);
      // check filename extension
      EXPECT_EQ(".sock", suffix.substr(suffix.length()-strlen(".sock")));
      std::string::size_type end = suffix.rfind('/');
      if (end == std::string::npos)
        end = suffix.rfind('\\');
      // check that the file is directly under the deployment directory
      EXPECT_EQ(suffix.substr(end+1), suffix);
    }
  }

  mysql_harness::reset_keyring();
  mysqlrouter::delete_recursive(dir);
}

static void bootstrap_overwrite_test(const std::string &dir,
                                     const std::string &name,
                                     bool force,
                                     const char *cluster_name,
                                     bool expect_fail,
                                     const std::map<std::string, std::string> &default_paths) {
  StrictMock<MySQLSessionReplayer> mysql;
  ::testing::InSequence s;

  ConfigGenerator config_gen;
    common_pass_metadata_checks(mysql);
  config_gen.init(&mysql);
  if (!expect_fail)
    expect_bootstrap_queries(mysql, cluster_name);
  else
    mysql.expect_query("").then_return(4, {{cluster_name, "myreplicaset", "pm", "somehost:3306"}});

  std::map<std::string, std::string> options;
  options["name"] = name;
  options["quiet"] = "1";
  if (force)
    options["force"] = "1";
  config_gen.bootstrap_directory_deployment(dir,
    options, default_paths, "delme", "delme.key");
}


TEST_F(ConfigGeneratorTest, bootstrap_overwrite) {
  std::string dir = "./configtest";

  // pre-cleanup just in case
  mysqlrouter::delete_recursive(dir);
  mysql_harness::reset_keyring();

  // Overwrite tests. Run bootstrap twice on the same output directory
  //
  // Name    --force     cluster_name   Expected
  // -------------------------------------------
  // same    no          same           OK (refreshing config)
  // same    no          diff           FAIL
  // same    yes         same           OK
  // same    yes         diff           OK (replacing config)
  // diff    no          same           OK
  // diff    no          diff           FAIL
  // diff    yes         same           OK
  // diff    yes         diff           OK
  //
  // diff name is just a rename, so no issue

  SCOPED_TRACE("bootstrap_overwrite1");
  // same    no          same           OK (refreshing config)
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_FALSE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_EQ(mysqlrouter::delete_recursive(dir), 0);

  SCOPED_TRACE("bootstrap_overwrite2");
  dir = "./configtest2";
  // same    no          diff           FAIL
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_THROW_LIKE(bootstrap_overwrite_test(dir, "myname", false, "kluster", true, default_paths),
                    std::runtime_error,
                    "If you'd like to replace it, please use the --force");
  mysql_harness::reset_keyring();
  ASSERT_FALSE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_EQ(mysqlrouter::delete_recursive(dir), 0);

  dir = "./configtest3";
  SCOPED_TRACE("bootstrap_overwrite3");
  // same    yes         same           OK
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", true, "cluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", true, "cluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_FALSE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_EQ(mysqlrouter::delete_recursive(dir), 0);

  dir = "./configtest4";
  SCOPED_TRACE("bootstrap_overwrite4");
  // same    yes         diff           OK (replacing config)
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", true, "kluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_TRUE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_EQ(mysqlrouter::delete_recursive(dir), 0);

  dir = "./configtest5";
  SCOPED_TRACE("bootstrap_overwrite5");
  // diff    no          same           OK (refreshing config)
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "xmyname", false, "cluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_TRUE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_EQ(mysqlrouter::delete_recursive(dir), 0);

  dir = "./configtest6";
  SCOPED_TRACE("bootstrap_overwrite6");
  // diff    no          diff           FAIL
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_THROW_LIKE(bootstrap_overwrite_test(dir, "xmyname", false, "kluster", true, default_paths),
                    std::runtime_error,
                    "If you'd like to replace it, please use the --force");
  mysql_harness::reset_keyring();
  ASSERT_FALSE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_EQ(mysqlrouter::delete_recursive(dir), 0);

  dir = "./configtest7";
  SCOPED_TRACE("bootstrap_overwrite7");
  // diff    yes         same           OK
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", true, "cluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "xmyname", true, "cluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_TRUE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_EQ(mysqlrouter::delete_recursive(dir), 0);

  dir = "./configtest8";
  SCOPED_TRACE("bootstrap_overwrite8");
  // diff    yes         diff           OK (replacing config)
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "xmyname", true, "kluster", false, default_paths));
  mysql_harness::reset_keyring();
  ASSERT_TRUE(mysql_harness::Path(dir).join("mysqlrouter.conf.bak").exists());
  ASSERT_EQ(mysqlrouter::delete_recursive(dir), 0);
}


static void test_key_length(const std::string &key,
                            const std::map<std::string, std::string> &default_paths) {
  using std::placeholders::_1;
  StrictMock<MySQLSessionReplayer> mysql;
  ::testing::InSequence s;

  mysqlrouter::set_prompt_password([key](const std::string&) -> std::string {
    return key;
  });
  ConfigGenerator config_gen;
  common_pass_metadata_checks(mysql);
  config_gen.init(&mysql);
  expect_bootstrap_queries(mysql, "mycluster");

  std::map<std::string, std::string> options;
  options["name"] = "test";
  options["quiet"] = "1";
  config_gen.bootstrap_directory_deployment("key_too_long",
      options, default_paths, "delme", "");
}

TEST_F(ConfigGeneratorTest, key_too_long) {
  ASSERT_FALSE(mysql_harness::Path("key_too_long").exists());

  // bug #24942008, keyring key too long
  ASSERT_NO_THROW(test_key_length(std::string(250, 'x'), default_paths));
  mysqlrouter::delete_recursive("key_too_long");
  mysql_harness::reset_keyring();

  ASSERT_NO_THROW(test_key_length(std::string(255, 'x'), default_paths));
  mysqlrouter::delete_recursive("key_too_long");
  mysql_harness::reset_keyring();

  ASSERT_THROW_LIKE(test_key_length(std::string(256, 'x'), default_paths),
    std::runtime_error,
    "too long");
  mysqlrouter::delete_recursive("key_too_long");
  mysql_harness::reset_keyring();

  ASSERT_THROW_LIKE(test_key_length(std::string(5000, 'x'), default_paths),
    std::runtime_error,
    "too long");
  mysqlrouter::delete_recursive("key_too_long");
  mysql_harness::reset_keyring();
}

TEST_F(ConfigGeneratorTest, bad_master_key) {
  // bug #24955928
  mysqlrouter::delete_recursive("./delme");
  // reconfiguring with an empty master key file throws an error referencing
  // the temporary file name instead of the actual name
  {
    StrictMock<MySQLSessionReplayer> mysql;
    ::testing::InSequence s;

    ConfigGenerator config_gen;
    common_pass_metadata_checks(mysql);
    config_gen.init(&mysql);
    expect_bootstrap_queries(mysql, "mycluster");

    std::map<std::string, std::string> options;
    options["name"] = "foo";
    options["quiet"] = "1";
    config_gen.bootstrap_directory_deployment("./delme",
        options, default_paths, "delme", "key");

    mysql_harness::reset_keyring();
  }
  {
    mysqlrouter::delete_file("delme/emptyfile");
    std::ofstream f("delme/emptyfile");
    StrictMock<MySQLSessionReplayer> mysql;
    ::testing::InSequence s;

    ConfigGenerator config_gen;
    common_pass_metadata_checks(mysql);
    config_gen.init(&mysql);
    expect_bootstrap_queries(mysql, "mycluster");

    std::map<std::string, std::string> options;
    options["name"] = "foo";
    options["quiet"] = "1";
    try {
      config_gen.bootstrap_directory_deployment("./delme",
          options, default_paths, "delme", "emptyfile");
      FAIL() << "Was expecting exception but got none\n";
    } catch (std::runtime_error &e) {
      if (strstr(e.what(), ".tmp"))
        FAIL() << "Exception text is: " << e.what() << "\n";
      std::string expected = std::string("Invalid master key file ");
      ASSERT_EQ(expected, std::string(e.what()).substr(0, expected.size()));
    }
  }
  mysqlrouter::delete_recursive("./delme");
  mysqlrouter::delete_file("emptyfile");
  mysql_harness::reset_keyring();
  // directory name but no filename
  {
    StrictMock<MySQLSessionReplayer> mysql;
    ::testing::InSequence s;

    ConfigGenerator config_gen;
    common_pass_metadata_checks(mysql);
    config_gen.init(&mysql);
    expect_bootstrap_queries(mysql, "mycluster");

    std::map<std::string, std::string> options;
    options["name"] = "foo";
    options["quiet"] = "1";
#ifdef __sun
    ASSERT_THROW_LIKE(
        config_gen.bootstrap_directory_deployment("./delme",
          options, default_paths, "delme", "."),
        std::runtime_error,
        ": Invalid argument");
#elif !defined(_WIN32)
    ASSERT_THROW_LIKE(
        config_gen.bootstrap_directory_deployment("./delme",
          options, default_paths, "delme", "."),
        std::runtime_error,
        ": Is a directory");
#else
    ASSERT_THROW_LIKE(
      config_gen.bootstrap_directory_deployment("./delme",
        options, default_paths, "delme", "."),
      std::runtime_error,
      "Permission denied");
#endif
  }
  mysqlrouter::delete_recursive("./delme");
  mysql_harness::reset_keyring();
}

TEST_F(ConfigGeneratorTest, full_test) {
  mysqlrouter::delete_recursive("./delme");

  StrictMock<MySQLSessionReplayer> mysql;
  ::testing::InSequence s;

  ConfigGenerator config_gen;
  common_pass_metadata_checks(mysql);
  config_gen.init(&mysql);
  expect_bootstrap_queries(mysql, "mycluster");

  std::map<std::string, std::string> options;
  options["name"] = "foo";
  options["quiet"] = "1";
  ASSERT_NO_THROW(
      config_gen.bootstrap_directory_deployment("./delme",
        options, default_paths, "delme", "masterkey"));

  std::string value;
  mysql_harness::Config config(mysql_harness::Config::allow_keys);
  config.read("delme/mysqlrouter.conf");

  value = config.get_default("master_key_path");
  EXPECT_TRUE(ends_with(value, "delme/masterkey"));

  value = config.get_default("name");
  EXPECT_EQ(value, "foo");

  value = config.get_default("keyring_path");
  EXPECT_EQ(mysql_harness::Path(value).basename(), "delme");

  mysqlrouter::delete_recursive("delme");
  mysql_harness::reset_keyring();
}

TEST_F(ConfigGeneratorTest, empty_config_file) {
  ConfigGenerator config;
  uint32_t router_id;
  const std::string test_dir("./delme");
  const std::string conf_path(test_dir + "/mysqlrouter.conf");

  mysqlrouter::delete_recursive(test_dir);
  mysqlrouter::mkdir(test_dir, 0700);

  std::ofstream file(conf_path, std::ofstream::out | std::ofstream::trunc);
  file.close();

  EXPECT_NO_THROW(
    std::tie(router_id, std::ignore) = config.get_router_id_and_name_from_config(conf_path, "dummy", false)
  );
  EXPECT_EQ(router_id, uint32_t(0));

  mysqlrouter::delete_recursive(test_dir);
  mysql_harness::reset_keyring();
}

TEST_F(ConfigGeneratorTest, ssl_stage1_cmdline_arg_parse) {

  // These tests verify that SSL options are handled correctly at argument parsing stage during bootstrap.
  // Note that at this stage, we only care about arguments being passed further down, and rely on mysql_*()
  // calls to deal with eventual inconsistencies. The only exception to this rule is parsing --ssl-mode,
  // which is a string that has to be converted to an SSL_MODE_* enum (though arguably that validation
  // could also be delayed).

  // --ssl-mode not given
  {                               //vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv {"-v", "--bootstrap", "0:3310" };
    MySQLRouter router(Path(), argv);
    EXPECT_EQ(0u, router.bootstrap_options_.count("ssl_mode"));
  }

  // --ssl-mode missing or empty argument
  {
    const std::vector<std::string> argument_required_options{"--ssl-mode",
        "--ssl-cipher", "--tls-version",
        "--ssl-ca", "--ssl-capath", "--ssl-crl", "--ssl-crlpath",
// 2017.01.26: Disabling this code, since it's not part of GA v2.1.2.  It should be re-enabled later
#if 0
        "--ssl-cert", "--ssl-key"
#endif
    };

    for (auto &opt : argument_required_options) {
                                           //vv---- vital!  We rely on it to exit out of MySQLRouter::init()
      const std::vector<std::string> argv {"-v", "--bootstrap", "0:3310", opt};
      try {
        MySQLRouter router(Path(), argv);
        FAIL() << "Expected std::invalid_argument to be thrown";
      } catch (const std::runtime_error &e) {
        EXPECT_STREQ(("option '"+opt+"' requires a value.").c_str(), e.what()); // TODO it would be nice to make case consistent
        SUCCEED();
      } catch (...) {
        FAIL() << "Expected std::runtime_error to be thrown";
      }

      // the value is required but also it CAN'T be empty, like when the user uses --tls-version ""
      const std::vector<std::string> argv2 {"-v", "--bootstrap", "0:3310", opt, ""};
      try {
        MySQLRouter router(Path(), argv2);
        FAIL() << "Expected std::invalid_argument to be thrown";
      } catch (const std::runtime_error &e) {
        if (opt == "--ssl-mode") {
          // The error for -ssl-mode is sligtly different than for other options - detected differently
          EXPECT_STREQ("Invalid value for --ssl-mode option", e.what());
        }
        else {
          EXPECT_STREQ(("Value for option '"+opt+"' can't be empty.").c_str(), e.what());
        }
        SUCCEED();
      } catch (...) {
        FAIL() << "Expected std::runtime_error to be thrown";
      }
    }
  }

  // --bootstrap missing
  {                               //vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv {"-v", "--ssl-mode", "whatever"};
    try {
      MySQLRouter router(Path(), argv);
      FAIL() << "Expected std::invalid_argument to be thrown";
    } catch (const std::runtime_error &e) {
      EXPECT_STREQ("Option --ssl-mode can only be used together with -B/--bootstrap", e.what());
      SUCCEED();
    } catch (...) {
      FAIL() << "Expected std::runtime_error to be thrown";
    }
  }

  // --ssl-mode has an invalid argument
  {                               //vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv {"-v", "--bootstrap", "0:3310", "--ssl-mode", "bad"};
    try {
      MySQLRouter router(Path(), argv);
      FAIL() << "Expected std::invalid_argument to be thrown";
    } catch (const std::runtime_error &e) {
      EXPECT_STREQ("Invalid value for --ssl-mode option", e.what());
      SUCCEED();
    } catch (...) {
      FAIL() << "Expected std::runtime_error to be thrown";
    }
  }

  // --ssl-mode = DISABLED + uppercase
  {                               //vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv {"-v", "--bootstrap", "0:3310", "--ssl-mode", "DISABLED"};
    MySQLRouter router(Path(), argv);
    EXPECT_EQ("DISABLED", router.bootstrap_options_.at("ssl_mode"));
  }

  // --ssl-mode = PREFERRED + lowercase
  {                               //vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv {"-v", "--bootstrap", "0:3310", "--ssl-mode", "preferred"};
    MySQLRouter router(Path(), argv);
    EXPECT_EQ("preferred", router.bootstrap_options_.at("ssl_mode"));
  }

  // --ssl-mode = REQUIRED + mixedcase
  {                               //vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv {"-v", "--bootstrap", "0:3310", "--ssl-mode", "rEqUIrEd"};
    MySQLRouter router(Path(), argv);
    EXPECT_EQ("rEqUIrEd", router.bootstrap_options_.at("ssl_mode"));
  }

  // --ssl-mode = VERIFY_CA
  {                               //vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv {"-v", "--bootstrap", "0:3310", "--ssl-mode", "verify_ca"};
    MySQLRouter router(Path(), argv);
    EXPECT_EQ("verify_ca", router.bootstrap_options_.at("ssl_mode"));
  }

  // --ssl-mode = VERIFY_CA, --ssl-ca etc
  {                               //vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv {"-v", "--bootstrap", "0:3310", "--ssl-mode", "verify_ca",
                                    "--ssl-ca=/some/ca.pem", "--ssl-capath=/some/cadir",
                                    "--ssl-crl=/some/crl.pem", "--ssl-crlpath=/some/crldir"};
    MySQLRouter router(Path(), argv);
    EXPECT_EQ("verify_ca", router.bootstrap_options_.at("ssl_mode"));
    EXPECT_EQ("/some/ca.pem", router.bootstrap_options_.at("ssl_ca"));
    EXPECT_EQ("/some/cadir", router.bootstrap_options_.at("ssl_capath"));
    EXPECT_EQ("/some/crl.pem", router.bootstrap_options_.at("ssl_crl"));
    EXPECT_EQ("/some/crldir", router.bootstrap_options_.at("ssl_crlpath"));
  }

  // --ssl-mode = VERIFY_IDENTITY, --ssl-ca etc
  {                               //vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv {"-v", "--bootstrap", "0:3310", "--ssl-mode", "verify_identity",
                                    "--ssl-ca=/some/ca.pem", "--ssl-capath=/some/cadir",
                                    "--ssl-crl=/some/crl.pem", "--ssl-crlpath=/some/crldir"};
    MySQLRouter router(Path(), argv);
    EXPECT_EQ("verify_identity", router.bootstrap_options_.at("ssl_mode"));
    EXPECT_EQ("/some/ca.pem", router.bootstrap_options_.at("ssl_ca"));
    EXPECT_EQ("/some/cadir", router.bootstrap_options_.at("ssl_capath"));
    EXPECT_EQ("/some/crl.pem", router.bootstrap_options_.at("ssl_crl"));
    EXPECT_EQ("/some/crldir", router.bootstrap_options_.at("ssl_crlpath"));
  }

  // --ssl-mode = REQUIRED, --ssl-* cipher options
  {                               //vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv {"-v", "--bootstrap", "0:3310", "--ssl-mode", "required",
                                   "--ssl-cipher",  "FOO-BAR-SHA678", "--tls-version", "TLSv1"};
    MySQLRouter router(Path(), argv);
    EXPECT_EQ("required", router.bootstrap_options_.at("ssl_mode"));
    EXPECT_EQ("FOO-BAR-SHA678", router.bootstrap_options_.at("ssl_cipher"));
    EXPECT_EQ("TLSv1", router.bootstrap_options_.at("tls_version"));
  }

// 2017.01.26: Disabling this code, since it's not part of GA v2.1.2.  It should be re-enabled later
#if 0
  // --ssl-mode = REQUIRED, --ssl-cert, --ssl-key
  {                               //vv---- vital!  We rely on it to exit out of MySQLRouter::init()
    std::vector<std::string> argv {"-v", "--bootstrap", "0:3310", "--ssl-mode", "required", "--ssl-cert=/some/cert.pem", "--ssl-key=/some/key.pem"};
    MySQLRouter router(Path(), argv);
    EXPECT_EQ("required", router.bootstrap_options_.at("ssl_mode"));
    EXPECT_EQ("/some/cert.pem", router.bootstrap_options_.at("ssl_cert"));
    EXPECT_EQ("/some/key.pem", router.bootstrap_options_.at("ssl_key"));
  }
#endif
}

TEST_F(ConfigGeneratorTest, ssl_stage2_bootstrap_connection) {

  // These tests verify that MySQLSession::set_ssl_options() gets called with appropriate
  // SSL options before making connection to metadata server during bootstrap

  StrictMock<ReplayerWithMockSSL> mock_mysql;
  set_mock_mysql(&mock_mysql);
  mysqlrouter::set_prompt_password([](const std::string&) -> std::string { return ""; });

  // mode
  {
    common_pass_metadata_checks(mock_mysql);
    ConfigGenerator config_gen;
    config_gen.init("", {{"ssl_mode", "DISABLED"}});   // DISABLED + uppercase
    EXPECT_EQ(mock_mysql.last_ssl_mode, SSL_MODE_DISABLED);
  }
  {
    common_pass_metadata_checks(mock_mysql);
    ConfigGenerator config_gen;
    config_gen.init("", {{"ssl_mode", "preferred"}});  // PREFERRED + lowercase
    EXPECT_EQ(mock_mysql.last_ssl_mode, SSL_MODE_PREFERRED);
  }
  {
    common_pass_metadata_checks(mock_mysql);
    ConfigGenerator config_gen;
    config_gen.init("", {{"ssl_mode", "rEqUIrEd"}});   // REQUIRED + mixedcase
    EXPECT_EQ(mock_mysql.last_ssl_mode, SSL_MODE_REQUIRED);
  }
  {
    common_pass_metadata_checks(mock_mysql);
    ConfigGenerator config_gen;
    config_gen.init("", {{"ssl_mode", "VERIFY_CA"}});
    EXPECT_EQ(mock_mysql.last_ssl_mode, SSL_MODE_VERIFY_CA);
  }
  {
    common_pass_metadata_checks(mock_mysql);
    ConfigGenerator config_gen;
    config_gen.init("", {{"ssl_mode", "VERIFY_IDENTITY"}});
    EXPECT_EQ(mock_mysql.last_ssl_mode, SSL_MODE_VERIFY_IDENTITY);
  }
  {
    // invalid ssl_mode should get handled at arg-passing stage, and so we have a unit test for that
    // in ssl_stage1_cmdline_arg_parse test above
  }

  // other fields
  {
    common_pass_metadata_checks(mock_mysql);
    ConfigGenerator config_gen;
    config_gen.init("", {
      {"ssl_ca",      "/some/ca/file"},
      {"ssl_capath",  "/some/ca/dir"},
      {"ssl_crl",     "/some/crl/file"},
      {"ssl_crlpath", "/some/crl/dir"},
      {"ssl_cipher",  "FOO-BAR-SHA678"},
      {"tls_version", "TLSv1"},
// 2017.01.26: Disabling this code, since it's not part of GA v2.1.2.  It should be re-enabled later
#if 0
      {"ssl_cert","/some/cert.pem"},
      {"ssl_key", "/some/key.pem"},
#endif
    });
    EXPECT_EQ(mock_mysql.last_ssl_ca,      "/some/ca/file");
    EXPECT_EQ(mock_mysql.last_ssl_capath,  "/some/ca/dir");
    EXPECT_EQ(mock_mysql.last_ssl_crl,     "/some/crl/file");
    EXPECT_EQ(mock_mysql.last_ssl_crlpath, "/some/crl/dir");
    EXPECT_EQ(mock_mysql.last_ssl_cipher,  "FOO-BAR-SHA678");
    EXPECT_EQ(mock_mysql.last_tls_version, "TLSv1");
// 2017.01.26: Disabling this code, since it's not part of GA v2.1.2.  It should be re-enabled later
#if 0
    EXPECT_EQ(mock_mysql.last_ssl_cert,    "/some/cert.pem");
    EXPECT_EQ(mock_mysql.last_ssl_key,     "/some/key.pem");
#endif
  }
}

TEST_F(ConfigGeneratorTest, ssl_stage3_create_config) {

  // These tests verify that config parameters passed to ConfigGenerator::create_config() will make
  // it to configuration file as expected. Note that even though ssl_mode options are not case-sensive,
  // their case should be preserved (written to config file exactly as given in bootstrap options).

  ConfigGenerator config_gen;

  auto test_config_output = [&config_gen](const std::map<std::string, std::string>& user_options, const char* result) {
    ConfigGenerator::Options options = config_gen.fill_options(false, user_options);
    std::stringstream output;
    config_gen.create_config(output, 123, "myrouter", "user", "server1,server2,server3",
                             "mycluster", "myreplicaset", "cluster_user", options);
    EXPECT_THAT(output.str(), HasSubstr(result));
  };

  test_config_output({{"ssl_mode", "DISABLED"}},  "ssl_mode=DISABLED");   // DISABLED + uppercase
  test_config_output({{"ssl_mode", "preferred"}}, "ssl_mode=preferred");  // PREFERRED + lowercase
  test_config_output({{"ssl_mode", "rEqUIrEd"}},  "ssl_mode=rEqUIrEd");   // REQUIRED + mixedcase
  test_config_output({{"ssl_mode", "Verify_Ca"}}, "ssl_mode=Verify_Ca");
  test_config_output({{"ssl_mode", "Verify_identity"}},  "ssl_mode=Verify_identity");

  test_config_output({{"ssl_ca", "/some/path"}}, "ssl_ca=/some/path");
  test_config_output({{"ssl_capath", "/some/path"}}, "ssl_capath=/some/path");
  test_config_output({{"ssl_crl", "/some/path"}}, "ssl_crl=/some/path");
  test_config_output({{"ssl_crlpath", "/some/path"}}, "ssl_crlpath=/some/path");
  test_config_output({{"ssl_cipher", "FOO-BAR-SHA678"}}, "ssl_cipher=FOO-BAR-SHA678");
  test_config_output({{"tls_version", "TLSv1"}}, "tls_version=TLSv1");
}

TEST_F(ConfigGeneratorTest, warn_on_no_ssl) {

  // These test warn_on_no_ssl(). For convenience, it returns true if no warning has been issued,
  // false if it issued a warning. And it throws if something went wrong.

  constexpr char kQuery[] = "show status like 'ssl_cipher'";
  ConfigGenerator config_gen;
  MySQLSessionReplayer mock_mysql;
  config_gen.mysql_ = &mock_mysql;

  // anything other than PREFERRED (or empty, which defaults to PREFERRED) should never warn.
  // warn_on_no_ssl() shouldn't even bother querying the database.
  {
    EXPECT_TRUE(config_gen.warn_on_no_ssl({{"ssl_mode", mysqlrouter::MySQLSession::kSslModeRequired}}));
    EXPECT_TRUE(config_gen.warn_on_no_ssl({{"ssl_mode", mysqlrouter::MySQLSession::kSslModeDisabled}}));
    EXPECT_TRUE(config_gen.warn_on_no_ssl({{"ssl_mode", mysqlrouter::MySQLSession::kSslModeVerifyCa}}));
    EXPECT_TRUE(config_gen.warn_on_no_ssl({{"ssl_mode", mysqlrouter::MySQLSession::kSslModeVerifyIdentity}}));
  }

  // run for 2 ssl_mode cases: unspecified and PREFERRED (they are equivalent)
  typedef std::map<std::string, std::string> Opts;
  for (Opts opt : { Opts{}, Opts{{"ssl_mode", mysqlrouter::MySQLSession::kSslModePreferred}} }) {

    { // have SLL
      mock_mysql.expect_query_one(kQuery).then_return(0, {{"ssl_cipher", "some_cipher"}});
      EXPECT_TRUE(config_gen.warn_on_no_ssl(opt));
    }

    { // don't have SLL - empty string
      mock_mysql.expect_query_one(kQuery).then_return(0, {{"ssl_cipher", ""}});
      EXPECT_FALSE(config_gen.warn_on_no_ssl(opt));
    }

    { // don't have SLL - null string
      mock_mysql.expect_query_one(kQuery).then_return(0, {{"ssl_cipher", nullptr}});
      EXPECT_FALSE(config_gen.warn_on_no_ssl(opt));
    }

    // CORNERCASES FOLLOW

    { // query failure
      mock_mysql.expect_query_one(kQuery).then_error("boo!", 1234);
      EXPECT_THROW(config_gen.warn_on_no_ssl(opt), std::runtime_error);
    }

    { // bogus query result - no columns
      mock_mysql.expect_query_one(kQuery).then_return(0, {});
      EXPECT_THROW(config_gen.warn_on_no_ssl(opt), std::runtime_error);
    }

    { // bogus query result - null column
      mock_mysql.expect_query_one(kQuery).then_return(0, {{nullptr}});
      EXPECT_THROW(config_gen.warn_on_no_ssl(opt), std::runtime_error);
    }

    { // bogus query result - 1 column
      mock_mysql.expect_query_one(kQuery).then_return(0, {{"foo"}});
      EXPECT_THROW(config_gen.warn_on_no_ssl(opt), std::runtime_error);
    }

    { // bogus query result - 1 column (ssl_cipher)
      mock_mysql.expect_query_one(kQuery).then_return(0, {{"ssl_cipher"}});
      EXPECT_THROW(config_gen.warn_on_no_ssl(opt), std::runtime_error);
    }

    { // bogus query result - 2 columns, but first is not ssl_cipher
      mock_mysql.expect_query_one(kQuery).then_return(0, {{"foo", "bar"}});
      EXPECT_THROW(config_gen.warn_on_no_ssl(opt), std::runtime_error);
    }
  }
}


TEST_F(ConfigGeneratorTest, warn_no_ssl_false) {
  MySQLSessionReplayer mock_mysql;

  const std::vector<std::string> prefered_values{"PREFERRED", "preferred", "Preferred"};
  for (size_t i = 0u; i < prefered_values.size(); ++i)
  {
    ConfigGenerator config_gen;

    common_pass_metadata_checks(mock_mysql);
    mock_mysql.expect_query_one("show status like 'ssl_cipher'");
    mock_mysql.then_return(2, {
        {mock_mysql.string_or_null("ssl_cipher"), mock_mysql.string_or_null("")}
      });

    std::map<std::string, std::string> options;
    options["ssl_mode"] = prefered_values[i];

    config_gen.init(&mock_mysql);
    const bool res = config_gen.warn_on_no_ssl(options);

    ASSERT_FALSE(res);
  }
}

TEST_F(ConfigGeneratorTest, warn_no_ssl_true) {
  MySQLSessionReplayer mock_mysql;

  {
    ConfigGenerator config_gen;

    common_pass_metadata_checks(mock_mysql);

    std::map<std::string, std::string> options;
    options["ssl_mode"] = "DISABLED";

    config_gen.init(&mock_mysql);
    const bool res = config_gen.warn_on_no_ssl(options);

    ASSERT_TRUE(res);
  }
}

TEST_F(ConfigGeneratorTest, set_file_owner_no_user) {
    ConfigGenerator config_gen;

    std::map<std::string, std::string> empty_options;
    ASSERT_NO_THROW (config_gen.set_file_owner(empty_options, "/tmp/somefile") );
}

TEST_F(ConfigGeneratorTest, set_file_owner_user_empty) {
    ConfigGenerator config_gen;

    std::map<std::string, std::string> bootstrap_options{{"user", ""}};
    ASSERT_NO_THROW (config_gen.set_file_owner(bootstrap_options, "/tmp/somefile") );
}


// bootstrap from URI/unix-socket/hostname checks
const std::string kDefaultUsername = "root";
const std::string kDefaultPassword = "";
const std::string kEmptyUnixSocket = "";
const uint16_t kDefaultMysqlPort = 0;

// passing a unix-socket path to --bootstrap should raise a runtime_error
TEST_F(ConfigGeneratorTest, bootstrap_from_unixsocket) {
  StrictMock<ReplayerWithMockSSL> mock_mysql;
  set_mock_mysql(&mock_mysql);
  mysqlrouter::set_prompt_password([](const std::string&) -> std::string { return kDefaultPassword; });

  mock_mysql.expect_connect("", kDefaultMysqlPort, kDefaultUsername, kDefaultPassword, "/tmp/mysql.sock");

  common_pass_metadata_checks(mock_mysql);

  ConfigGenerator config_gen;
  EXPECT_THROW({
      config_gen.init("/tmp/mysql.sock", {});
      },
      std::runtime_error);
}

TEST_F(ConfigGeneratorTest, bootstrap_from_ipv6) {
  StrictMock<ReplayerWithMockSSL> mock_mysql;
  set_mock_mysql(&mock_mysql);
  mysqlrouter::set_prompt_password([](const std::string&) -> std::string { return ""; });

  mock_mysql.expect_connect("::1", kDefaultMysqlPort, kDefaultUsername, kDefaultPassword, kEmptyUnixSocket);
  common_pass_metadata_checks(mock_mysql);

  ConfigGenerator config_gen;
  config_gen.init("[::1]", {});
}

TEST_F(ConfigGeneratorTest, bootstrap_from_ipv6_with_port) {
  StrictMock<ReplayerWithMockSSL> mock_mysql;
  set_mock_mysql(&mock_mysql);
  mysqlrouter::set_prompt_password([](const std::string&) -> std::string { return ""; });

  mock_mysql.expect_connect("::1", 3306, kDefaultUsername, kDefaultPassword, kEmptyUnixSocket);
  common_pass_metadata_checks(mock_mysql);

  ConfigGenerator config_gen;
  config_gen.init("[::1]:3306", {});
}

TEST_F(ConfigGeneratorTest, bootstrap_from_hostname) {
  StrictMock<ReplayerWithMockSSL> mock_mysql;
  set_mock_mysql(&mock_mysql);
  mysqlrouter::set_prompt_password([](const std::string&) -> std::string { return ""; });

  mock_mysql.expect_connect("127.0.0.1", 0, kDefaultUsername, kDefaultPassword, kEmptyUnixSocket);
  common_pass_metadata_checks(mock_mysql);

  ConfigGenerator config_gen;
  config_gen.init("localhost", {});
}

TEST_F(ConfigGeneratorTest, bootstrap_from_hostname_with_port) {
  StrictMock<ReplayerWithMockSSL> mock_mysql;
  set_mock_mysql(&mock_mysql);
  mysqlrouter::set_prompt_password([](const std::string&) -> std::string { return ""; });

  mock_mysql.expect_connect("127.0.0.1", 3306, kDefaultUsername, kDefaultPassword, kEmptyUnixSocket);
  common_pass_metadata_checks(mock_mysql);

  ConfigGenerator config_gen;
  config_gen.init("localhost:3306", {});
}

TEST_F(ConfigGeneratorTest, bootstrap_from_uri) {
  StrictMock<ReplayerWithMockSSL> mock_mysql;
  set_mock_mysql(&mock_mysql);
  mysqlrouter::set_prompt_password([](const std::string&) -> std::string { return ""; });

  mock_mysql.expect_connect("127.0.0.1", 3306, kDefaultUsername, kDefaultPassword, kEmptyUnixSocket);
  common_pass_metadata_checks(mock_mysql);

  ConfigGenerator config_gen;
  config_gen.init("mysql://localhost:3306/", {});
}

TEST_F(ConfigGeneratorTest, bootstrap_from_uri_unixsocket) {
  StrictMock<ReplayerWithMockSSL> mock_mysql;
  set_mock_mysql(&mock_mysql);
  mysqlrouter::set_prompt_password([](const std::string&) -> std::string { return ""; });

  mock_mysql.expect_connect("localhost", 3306, kDefaultUsername, kDefaultPassword, "/tmp/mysql.sock");
  common_pass_metadata_checks(mock_mysql);

  ConfigGenerator config_gen;
  EXPECT_NO_THROW({
      config_gen.init("mysql://localhost:3306/", {{"bootstrap_socket", "/tmp/mysql.sock"}});
      });
}

// a invalid URI (port too large) should trigger a expection
TEST_F(ConfigGeneratorTest, bootstrap_from_invalid_uri) {
  StrictMock<ReplayerWithMockSSL> mock_mysql;
  set_mock_mysql(&mock_mysql);
  mysqlrouter::set_prompt_password([](const std::string&) -> std::string { return ""; });

  common_pass_metadata_checks(mock_mysql);

  ConfigGenerator config_gen;
  EXPECT_THROW({
      config_gen.init("mysql://localhost:330660/", {{"bootstrap_socket", "/tmp/mysql.sock"}});
      },
      std::runtime_error);
}

// if socket-name is specified, the hostname in the bootstrap-uri has to be 'localhost'
TEST_F(ConfigGeneratorTest, bootstrap_fail_if_socket_and_hostname) {
  StrictMock<ReplayerWithMockSSL> mock_mysql;
  set_mock_mysql(&mock_mysql);
  mysqlrouter::set_prompt_password([](const std::string&) -> std::string { return ""; });

  common_pass_metadata_checks(mock_mysql);

  ConfigGenerator config_gen;
  EXPECT_THROW({
      config_gen.init("somehost", {{"bootstrap_socket", "/tmp/mysql.sock"}});
      },
      std::runtime_error);
}

// if socket-name is specified and hostname is 'localhost' then  bootstrap should work
TEST_F(ConfigGeneratorTest, bootstrap_if_socket_and_localhost) {
  StrictMock<ReplayerWithMockSSL> mock_mysql;
  set_mock_mysql(&mock_mysql);
  mysqlrouter::set_prompt_password([](const std::string&) -> std::string { return ""; });

  mock_mysql.expect_connect("localhost", 0, kDefaultUsername, kDefaultPassword, "/tmp/mysql.sock");
  common_pass_metadata_checks(mock_mysql);

  ConfigGenerator config_gen;
  EXPECT_NO_THROW({
      config_gen.init("localhost", {{"bootstrap_socket", "/tmp/mysql.sock"}});
      });
}

static void bootstrap_password_test(const std::string &dir,
                           const std::map<std::string, std::string> &default_paths,
                           const std::vector<query_entry_t> &bootstrap_queries,
                           std::string password_retries = "5",
                           bool force_password_validation = false) {
  StrictMock<MySQLSessionReplayer> mysql;
  ConfigGenerator config_gen;
  ::testing::InSequence s;
  common_pass_metadata_checks(mysql);
  config_gen.init(&mysql);
  expect_bootstrap_queries(mysql, "mycluster", bootstrap_queries);

  std::map<std::string, std::string> options;
  options["name"] = "name";
  options["password-retries"] = password_retries;
  if (force_password_validation)
    options["force-password-validation"] = "1";

  std::shared_ptr<void> exit_guard(nullptr, [&](void*){
    mysqlrouter::delete_recursive(dir);
    mysql_harness::reset_keyring();});

  config_gen.bootstrap_directory_deployment(dir,
      options, default_paths, "delme", "delme.key");
}

TEST_F(ConfigGeneratorTest, bootstrap_generate_password_force_password_validation) {
  const std::string kDirName = "./gen_pass_test";

  // copy expected bootstrap queries brefore CREATE USER
  std::vector<query_entry_t> bootstrap_queries;
  for (unsigned i = 0; i < 5; ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // we expect the user to be created without using HASHed password
  // and mysql_native_password plugin as we are forcing password validation
  bootstrap_queries.push_back( {"CREATE USER mysql_router4_012345678901@'%'"
                                " IDENTIFIED BY", ACTION_EXECUTE } );

  // copy the remaining bootstrap queries
  for (unsigned i = 6; i < expected_bootstrap_queries.size(); ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  bootstrap_password_test(kDirName, default_paths, bootstrap_queries, "5", true /*force_password_validation*/);
}

TEST_F(ConfigGeneratorTest, bootstrap_generate_password_no_native_plugin) {
  const std::string kDirName = "./gen_pass_test";

  // copy expected bootstrap queries brefore CREATE USER
  std::vector<query_entry_t> bootstrap_queries;
  for (unsigned i = 0; i < 5; ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // emulate error 1524 (plugin not loaded) after the call to first CREATE USER
  bootstrap_queries.push_back( {"CREATE USER mysql_router4_012345678901@'%'"
                                " IDENTIFIED WITH mysql_native_password AS", ACTION_ERROR, 0, 1524 } );

  // that should lead to rollback and retry witout hashed password
  bootstrap_queries.push_back( {"ROLLBACK", ACTION_EXECUTE } );

  bootstrap_queries.push_back( {"DROP USER IF EXISTS mysql_router4_012345678901@'%'", ACTION_EXECUTE} );
  bootstrap_queries.push_back( {"CREATE USER mysql_router4_012345678901@'%'"
                                " IDENTIFIED BY", ACTION_EXECUTE } );

  // copy the remaining bootstrap queries
  for (unsigned i = 6; i < expected_bootstrap_queries.size(); ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  bootstrap_password_test(kDirName, default_paths, bootstrap_queries);
}

TEST_F(ConfigGeneratorTest, bootstrap_generate_password_retry_ok)  {
  const std::string kDirName = "./gen_pass_test";

  // copy expected bootstrap queries brefore CREATE USER
  std::vector<query_entry_t> bootstrap_queries;
  for (unsigned i = 0; i < 5; ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // emulate error 1524 (plugin not loaded) after the call to first CREATE USER
  bootstrap_queries.push_back( {"CREATE USER mysql_router4_012345678901@'%'"
                                " IDENTIFIED WITH mysql_native_password AS", ACTION_ERROR, 0, 1524 } );

  // that should lead to rollback and retry witout hashed password
  bootstrap_queries.push_back( {"ROLLBACK", ACTION_EXECUTE } );

  // emulate error 1819) (password does not satisfy the current policy requirements) after the call to second CREATE USER
  bootstrap_queries.push_back( {"DROP USER IF EXISTS mysql_router4_012345678901@'%'", ACTION_EXECUTE} );
  bootstrap_queries.push_back( {"CREATE USER mysql_router4_012345678901@'%'"
                                " IDENTIFIED BY", ACTION_ERROR, 0, 1819 } );

  // that should lead to rollback and another retry witout hashed password
  bootstrap_queries.push_back( {"ROLLBACK", ACTION_EXECUTE } );

  bootstrap_queries.push_back( {"DROP USER IF EXISTS mysql_router4_012345678901@'%'", ACTION_EXECUTE} );
  bootstrap_queries.push_back( {"CREATE USER mysql_router4_012345678901@'%'"
                                " IDENTIFIED BY", ACTION_EXECUTE } );

  // copy the remaining bootstrap queries
  for (unsigned i = 6; i < expected_bootstrap_queries.size(); ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  bootstrap_password_test(kDirName, default_paths, bootstrap_queries);
}

TEST_F(ConfigGeneratorTest, bootstrap_generate_password_retry_failed) {
  const std::string kDirName = "./gen_pass_test";
  const unsigned kPasswordRetries = 3;

  // copy expected bootstrap queries brefore CREATE USER
  std::vector<query_entry_t> bootstrap_queries;
  for (unsigned i = 0; i < 5; ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }

  // emulate error 1524 (plugin not loaded) after the call to first CREATE USER
  bootstrap_queries.push_back( {"CREATE USER mysql_router4_012345678901@'%'"
                                " IDENTIFIED WITH mysql_native_password AS", ACTION_ERROR, 0, 1524 } );

  // that should lead to rollback and retry witout hashed password for "kPasswordRetries" number of times
  for (unsigned i = 0; i < kPasswordRetries; ++i) {
    bootstrap_queries.push_back( {"ROLLBACK", ACTION_EXECUTE } );

    bootstrap_queries.push_back( {"DROP USER IF EXISTS mysql_router4_012345678901@'%'", ACTION_EXECUTE} );
    // each time emulate error 1819) (password does not satisfy the current policy requirements) after the call to second CREATE USER
    bootstrap_queries.push_back( {"CREATE USER mysql_router4_012345678901@'%'"
                                 " IDENTIFIED BY", ACTION_ERROR, 0, 1819 } );
  }
  bootstrap_queries.push_back( {"ROLLBACK", ACTION_EXECUTE } );

  try {
    bootstrap_password_test(kDirName, default_paths, bootstrap_queries, std::to_string(kPasswordRetries));
    FAIL() << "Expecting exception";
  }
  catch (const std::runtime_error& exc) {

    ASSERT_NE(std::string::npos,
              std::string(exc.what()).find("Try to decrease the validate_password rules and try the operation again."));
  }
}

TEST_F(ConfigGeneratorTest, bootstrap_password_retry_param_wrong_values) {
  const std::string kDirName = "./gen_pass_test";
  std::vector<query_entry_t> bootstrap_queries;
  for (unsigned i = 0; i < 5; ++i) {
    bootstrap_queries.push_back(expected_bootstrap_queries.at(i));
  }
  // emulate error 1524 (plugin not loaded) after the call to first CREATE USER
  bootstrap_queries.push_back( {"CREATE USER mysql_router4_012345678901@'%'"
                                " IDENTIFIED WITH mysql_native_password AS", ACTION_ERROR, 0, 1524 } );
  bootstrap_queries.push_back( {"ROLLBACK", ACTION_EXECUTE } );

  // without --bootstrap
  {
    const std::vector<std::string> argv {"--password-retries", "2"};
    try {
      MySQLRouter router(Path(), argv);
      FAIL() << "Expected exception";
    } catch (const std::runtime_error &e) {
      EXPECT_STREQ("Option --password-retries can only be used together with -B/--bootstrap", e.what());
    }
  }

  // value too small
  {
    try {
      bootstrap_password_test(kDirName, default_paths, bootstrap_queries, "0");
      FAIL() << "Expecting exception";
    }
    catch (const std::runtime_error& exc) {
      EXPECT_STREQ("Invalid password-retries value '0'; please pick a value from 1 to 10000", exc.what());
    }
  }

  // value too big
  {
    try {
      bootstrap_password_test(kDirName, default_paths, bootstrap_queries, "999999");
      FAIL() << "Expecting exception";
    }
    catch (const std::runtime_error& exc) {
      EXPECT_STREQ("Invalid password-retries value '999999'; please pick a value from 1 to 10000", exc.what());
    }
  }

  // value wrong type
  {
    try {
      bootstrap_password_test(kDirName, default_paths, bootstrap_queries, "foo");
      FAIL() << "Expecting exception";
    }
    catch (const std::runtime_error& exc) {
      EXPECT_STREQ("Invalid password-retries value 'foo'; please pick a value from 1 to 10000", exc.what());
    }
  }

  // value empty
  {
    try {
      bootstrap_password_test(kDirName, default_paths, bootstrap_queries, "");
      FAIL() << "Expecting exception";
    }
    catch (const std::runtime_error& exc) {
      EXPECT_STREQ("Invalid password-retries value ''; please pick a value from 1 to 10000", exc.what());
    }
  }
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin = mysql_harness::Path(argv[0]).dirname();
  g_cwd = mysql_harness::Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
