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

#include "router_test_helpers.h"
#include "mysqlrouter/utils.h"
#include "config_parser.h"
#include "gtest_consoleoutput.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <streambuf>
#include "keyring/keyring_manager.h"
#ifdef _WIN32
#include <Winsock2.h>
#else
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

#include "config_generator.h"
#include "mysqlrouter/mysql_session.h"
#include "mysql_session_replayer.h"
#include "cluster_metadata.h"

std::string g_cwd;
mysql_harness::Path g_origin;

class ConfigGeneratorTest : public ConsoleOutputTest {
protected:
  virtual void SetUp() {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(g_cwd));
    config_path->append("Bug24570426.ini");

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
  // using hostname queried locally
  {
    MySQLSessionReplayer mock_mysql;

    ::testing::InSequence s;
    common_pass_metadata_checks(mock_mysql);
    mock_mysql.expect_execute("DROP USER IF EXISTS cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("CREATE USER cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("GRANT SELECT ON mysql_innodb_cluster_metadata.* TO cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("GRANT SELECT ON performance_schema.replication_group_members TO cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("GRANT SELECT ON performance_schema.replication_group_member_stats TO cluster_user@'%'").then_ok();

    ConfigGenerator config_gen;
    config_gen.init(&mock_mysql);
    config_gen.create_account("cluster_user", "secret");
  }
  // using IP queried from PFS
  {
    MySQLSessionReplayer mock_mysql;
    //std::vector<const char*> result{"::fffffff:123.45.67.8"};

    ::testing::InSequence s;
    common_pass_metadata_checks(mock_mysql);
    mock_mysql.expect_execute("DROP USER IF EXISTS cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("CREATE USER cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("GRANT SELECT ON mysql_innodb_cluster_metadata.* TO cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("GRANT SELECT ON performance_schema.replication_group_members TO cluster_user@'%'").then_ok();
    mock_mysql.expect_execute("GRANT SELECT ON performance_schema.replication_group_member_stats TO cluster_user@'%'").then_ok();

    ConfigGenerator config_gen;
    config_gen.init(&mock_mysql);
    config_gen.create_account("cluster_user", "secret");
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
  }
}

//XXX TODO: add recursive directory delete function

static struct {
  const char *query;
  bool execute;
} expected_bootstrap_queries[] = {
  {"START TRANSACTION", true},
  {"SELECT host_id, host_name", false},
  {"INSERT INTO mysql_innodb_cluster_metadata.hosts", true},
  {"INSERT INTO mysql_innodb_cluster_metadata.routers", true},
  {"DROP USER IF EXISTS mysql_innodb_cluster_router0@'%'", true},
  {"CREATE USER mysql_innodb_cluster_router0@'%'", true},
  {"GRANT SELECT ON mysql_innodb_cluster_metadata.* TO mysql_innodb_cluster_router0@'%'", true},
  {"GRANT SELECT ON performance_schema.replication_group_members TO mysql_innodb_cluster_router0@'%'", true},
  {"GRANT SELECT ON performance_schema.replication_group_member_stats TO mysql_innodb_cluster_router0@'%'", true},
  {"UPDATE mysql_innodb_cluster_metadata.routers SET attributes = ", true},
  {"COMMIT", true},
  {NULL, true}
};

static void expect_bootstrap_queries(MySQLSessionReplayer &m, const char *cluster_name) {
  m.expect_query("").then_return(4, {{cluster_name, "myreplicaset", "pm", "somehost:3306"}});
  for (int i = 0; expected_bootstrap_queries[i].query; i++) {
    if (expected_bootstrap_queries[i].execute)
      m.expect_execute(expected_bootstrap_queries[i].query).then_ok();
    else
      m.expect_query_one(expected_bootstrap_queries[i].query).then_return(2, {});
  }
}


static void bootstrap_name_test(const std::string &dir,
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
  config_gen.bootstrap_directory_deployment(dir,
      options, default_paths, "delme", "delme.key");
}


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
    mysqlrouter::delete_recursive("./delme");
    mysqlrouter::delete_file("emptyfile");
    mysql_harness::reset_keyring();
  }
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


    mysqlrouter::delete_recursive("./delme");
    mysql_harness::reset_keyring();
  }
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
    router_id = config.get_router_id_from_config_file(conf_path, "dummy",
                                                      false)
  );
  EXPECT_EQ(router_id, uint32_t(0));

  mysqlrouter::delete_recursive(test_dir);
  mysql_harness::reset_keyring();
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin = mysql_harness::Path(argv[0]).dirname();
  g_cwd = mysql_harness::Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
