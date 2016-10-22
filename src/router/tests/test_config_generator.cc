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

#include <cstring>
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

#include "mysqlrouter/mysql_session.h"

#include <gtest/gtest_prod.h>
#include "config_generator.h"


class MockMySQLSession : public mysqlrouter::MySQLSession {
public:
  MOCK_METHOD5(connect, void(const std::string &host, unsigned int port,
                             const std::string &username,
                             const std::string &password,
                             int connection_timeout));
  MOCK_METHOD0(disconnect, void());

  MOCK_METHOD1(execute, void(const std::string &query));

  MOCK_METHOD1(query_one, ResultRow *(const std::string &query));

  virtual void query(const std::string &q, const RowProcessor &proc) {
    (void)q;
    for (auto row : query_rows_)
      proc(row);
    query_rows_.clear();
  }

  void feed_query_row(const std::vector<const char*> &row) {
    query_rows_.push_back(row);
  }

private:
  std::vector<std::vector<const char*>> query_rows_;
};


class ResultRow : public mysqlrouter::MySQLSession::ResultRow {
public:
  ResultRow(const mysqlrouter::MySQLSession::Row &row) {
    row_ = row;
  }
};


class ConfigGeneratorTest : public ::testing::Test {
protected:
  virtual void SetUp() {
  }
};


using ::testing::Return;
using namespace testing;


TEST_F(ConfigGeneratorTest, fetch_bootstrap_servers_one) {
  MockMySQLSession mock_mysql;

  std::string primary_cluster_name_;
  std::string primary_replicaset_servers_;
  std::string primary_replicaset_name_;
  bool multi_master_ = false;

  {
    ConfigGenerator config_gen;
    EXPECT_CALL(mock_mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    config_gen.init(&mock_mysql);

    // "F.cluster_name, "
    // "R.replicaset_name, "
    // "R.topology_type, "
    // "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic')) "
    mock_mysql.feed_query_row({"mycluster", "myreplicaset", "pm", "somehost:3306"});

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
    EXPECT_CALL(mock_mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    config_gen.init(&mock_mysql);

    mock_mysql.feed_query_row({"mycluster", "myreplicaset", "mm", "somehost:3306"});

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
    EXPECT_CALL(mock_mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    config_gen.init(&mock_mysql);

    mock_mysql.feed_query_row({"mycluster", "myreplicaset", "xxx", "somehost:3306"});

    ASSERT_THROW(
      config_gen.fetch_bootstrap_servers(
        primary_replicaset_servers_,
        primary_cluster_name_, primary_replicaset_name_, multi_master_),
      std::runtime_error);
  }
}

TEST_F(ConfigGeneratorTest, fetch_bootstrap_servers_three) {
  MockMySQLSession mock_mysql;

  std::string primary_cluster_name_;
  std::string primary_replicaset_servers_;
  std::string primary_replicaset_name_;
  bool multi_master_ = false;

  {
    ConfigGenerator config_gen;
    EXPECT_CALL(mock_mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    config_gen.init(&mock_mysql);

    // "F.cluster_name, "
    // "R.replicaset_name, "
    // "R.topology_type, "
    // "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic')) "
    mock_mysql.feed_query_row({"mycluster", "myreplicaset", "pm", "somehost:3306"});
    mock_mysql.feed_query_row({"mycluster", "myreplicaset", "pm", "otherhost:3306"});
    mock_mysql.feed_query_row({"mycluster", "myreplicaset", "pm", "sumhost:3306"});

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
  MockMySQLSession mock_mysql;

  std::string primary_cluster_name_;
  std::string primary_replicaset_servers_;
  std::string primary_replicaset_name_;
  bool multi_master_ = false;

  {
    ConfigGenerator config_gen;
    EXPECT_CALL(mock_mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    config_gen.init(&mock_mysql);

    mock_mysql.feed_query_row({"mycluster", "myreplicaset", "pm", "somehost:3306"});
    mock_mysql.feed_query_row({"mycluster", "anotherreplicaset", "pm", "otherhost:3306"});

    ASSERT_THROW(
      config_gen.fetch_bootstrap_servers(
        primary_replicaset_servers_,
        primary_cluster_name_, primary_replicaset_name_, multi_master_),
      std::runtime_error);
  }

  {
    ConfigGenerator config_gen;
    EXPECT_CALL(mock_mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    config_gen.init(&mock_mysql);

    mock_mysql.feed_query_row({"mycluster", "myreplicaset", "pm", "somehost:3306"});
    mock_mysql.feed_query_row({"anothercluster", "anotherreplicaset", "pm", "otherhost:3306"});

    ASSERT_THROW(
      config_gen.fetch_bootstrap_servers(
        primary_replicaset_servers_,
        primary_cluster_name_, primary_replicaset_name_, multi_master_),
      std::runtime_error);
  }
}


TEST_F(ConfigGeneratorTest, fetch_bootstrap_servers_invalid) {
  MockMySQLSession mock_mysql;

  std::string primary_cluster_name_;
  std::string primary_replicaset_servers_;
  std::string primary_replicaset_name_;
  bool multi_master_ = false;

  {
    ConfigGenerator config_gen;
    EXPECT_CALL(mock_mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    config_gen.init(&mock_mysql);

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
    MockMySQLSession mock_mysql;

    ::testing::InSequence s;
    EXPECT_CALL(mock_mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    EXPECT_CALL(mock_mysql, execute(HasSubstr("DROP USER IF EXISTS cluster_user@")));
    EXPECT_CALL(mock_mysql, execute(HasSubstr("CREATE USER cluster_user@")));
    EXPECT_CALL(mock_mysql, execute(HasSubstr("GRANT SELECT ON mysql_innodb_cluster_metadata.* TO cluster_user@'")));
    EXPECT_CALL(mock_mysql, execute(HasSubstr("GRANT SELECT ON performance_schema.replication_group_members TO cluster_user@'")));

    ConfigGenerator config_gen;
    config_gen.init(&mock_mysql);
    config_gen.create_account("cluster_user", "secret");
  }
  // using IP queried from PFS
  {
    MockMySQLSession mock_mysql;
    //std::vector<const char*> result{"::fffffff:123.45.67.8"};

    ::testing::InSequence s;
    EXPECT_CALL(mock_mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    EXPECT_CALL(mock_mysql, execute(HasSubstr("DROP USER IF EXISTS cluster_user@'%'")));
    EXPECT_CALL(mock_mysql, execute(HasSubstr("CREATE USER cluster_user@'%'")));
    EXPECT_CALL(mock_mysql, execute(HasSubstr("GRANT SELECT ON mysql_innodb_cluster_metadata.* TO cluster_user@'%'")));
    EXPECT_CALL(mock_mysql, execute(HasSubstr("GRANT SELECT ON performance_schema.replication_group_members TO cluster_user@'%'")));

    ConfigGenerator config_gen;
    config_gen.init(&mock_mysql);
    config_gen.create_account("cluster_user", "secret");
  }
}


TEST_F(ConfigGeneratorTest, create_config_single_master) {
  StrictMock<MockMySQLSession> mock_mysql;

  std::map<std::string, std::string> user_options;

  ConfigGenerator config_gen;
  EXPECT_CALL(mock_mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
  config_gen.init(&mock_mysql);
  ConfigGenerator::Options options = config_gen.fill_options(false, user_options);

  {
    std::stringstream output;
    config_gen.create_config(output,
                        123, "myrouter",
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
                        123, "",
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
                        123, "",
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
                        123, "",
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
                        123, "",
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
                        123, "myrouter",
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
  StrictMock<MockMySQLSession> mock_mysql;

  std::map<std::string, std::string> user_options;

  ConfigGenerator config_gen;
  EXPECT_CALL(mock_mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
  config_gen.init(&mock_mysql);
  ConfigGenerator::Options options = config_gen.fill_options(true, user_options);
  config_gen.create_config(output,
                      123, "myrouter",
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
  StrictMock<MockMySQLSession> mock_mysql;

  ConfigGenerator config_gen;
  EXPECT_CALL(mock_mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
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
    user_options["base-port"] = "65535";
    options = config_gen.fill_options(false, user_options);
    ASSERT_THAT(options.rw_endpoint.port, Eq(65535));
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
  {"UPDATE mysql_innodb_cluster_metadata.routers SET attributes = ", true},
  {"COMMIT", true},
  {NULL, true}
};

static void expect_bootstrap_queries(MockMySQLSession &m, const char *cluster_name) {
  for (int i = 0; expected_bootstrap_queries[i].query; i++) {
    if (expected_bootstrap_queries[i].execute)
      EXPECT_CALL(m, execute(StartsWith(expected_bootstrap_queries[i].query)));
    else
      EXPECT_CALL(m, query_one(StartsWith(expected_bootstrap_queries[i].query)));
  }
  m.feed_query_row({cluster_name, "myreplicaset", "pm", "somehost:3306"});
}


static void bootstrap_name_test(const std::string &dir,
                           const std::string &name,
                           bool expect_fail) {
  StrictMock<MockMySQLSession> mysql;
  ::testing::InSequence s;

  ConfigGenerator config_gen;
  EXPECT_CALL(mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
  config_gen.init(&mysql);
  if (!expect_fail)
    expect_bootstrap_queries(mysql, "mycluster");

  std::map<std::string, std::string> options;
  options["name"] = name;
  options["quiet"] = "1";
  config_gen.bootstrap_directory_deployment(dir,
      options, "delme", dir+"/delme.key");
}


TEST_F(ConfigGeneratorTest, bootstrap_invalid_name) {
  const std::string dir = "bug24807941";
  mysqlrouter::delete_recursive(dir);

  // Bug#24807941
  ASSERT_NO_THROW(bootstrap_name_test(dir, "myname", false));
  mysqlrouter::delete_recursive(dir);
  mysql_harness::reset_keyring();

  ASSERT_NO_THROW(bootstrap_name_test(dir, "myname", false));
  mysqlrouter::delete_recursive(dir);
  mysql_harness::reset_keyring();

  ASSERT_NO_THROW(bootstrap_name_test(dir, "", false));
  mysqlrouter::delete_recursive(dir);
  mysql_harness::reset_keyring();

  ASSERT_THROW_LIKE(
    bootstrap_name_test(dir, "system", true),
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
      bootstrap_name_test(dir, name, true),
      std::runtime_error,
      "Router name '"+name+"' contains invalid characters.");
    mysqlrouter::delete_recursive(dir);
    mysql_harness::reset_keyring();
  }

  ASSERT_THROW_LIKE(
    bootstrap_name_test(dir, "veryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryverylongname", true),
    std::runtime_error,
    "too long (max 255).");
  mysqlrouter::delete_recursive(dir);
  mysql_harness::reset_keyring();
}


ACTION(ThrowMySQLError) {
  throw mysqlrouter::MySQLSession::Error("boo!", 1234);
}


TEST_F(ConfigGeneratorTest, bootstrap_cleanup_on_failure) {
  const std::string dir = "bug24808634";
  mysqlrouter::delete_recursive(dir);
  mysqlrouter::delete_file("delme.key");

  ASSERT_FALSE(mysql_harness::Path(dir).exists());
  ASSERT_FALSE(mysql_harness::Path("delme.key").exists());
  // cleanup on failure when dir didn't exist before
  {
    StrictMock<MockMySQLSession> mysql;
    ::testing::InSequence s;

    ConfigGenerator config_gen;
    EXPECT_CALL(mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    config_gen.init(&mysql);
    mysql.feed_query_row({"mycluter", "myreplicaset", "pm", "somehost:3306"});
    // force a failure during account creationg
    EXPECT_CALL(mysql, execute(_)).WillOnce(ThrowMySQLError());

    std::map<std::string, std::string> options;
    options["name"] = "foobar";
    options["quiet"] = "1";
    ASSERT_THROW(
      config_gen.bootstrap_directory_deployment(dir,
          options, "delme", "delme.key"),
      mysqlrouter::MySQLSession::Error);

    ASSERT_FALSE(mysql_harness::Path(dir).exists());
    ASSERT_FALSE(mysql_harness::Path("delme.key").exists());
  }
  mysql_harness::reset_keyring();

  // this should succeed, so that we can test that cleanup doesn't delete existing stuff
  {
    StrictMock<MockMySQLSession> mysql;
    ::testing::InSequence s;

    ConfigGenerator config_gen;
    EXPECT_CALL(mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    config_gen.init(&mysql);
    expect_bootstrap_queries(mysql, "mycluster");

    std::map<std::string, std::string> options;
    options["name"] = "foobar";
    options["quiet"] = "1";
    ASSERT_NO_THROW(
      config_gen.bootstrap_directory_deployment(dir,
          options, "delme", "delme.key"));

    ASSERT_TRUE(mysql_harness::Path(dir).exists());
    ASSERT_TRUE(mysql_harness::Path("delme.key").exists());
  }
  mysql_harness::reset_keyring();

  // don't cleanup on failure if dir already existed before
  {
    StrictMock<MockMySQLSession> mysql;
    ::testing::InSequence s;

    ConfigGenerator config_gen;
    EXPECT_CALL(mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    config_gen.init(&mysql);
    mysql.feed_query_row({"mycluster", "myreplicaset", "pm", "somehost:3306"});
    // force a failure during account creationg
    EXPECT_CALL(mysql, execute(_)).WillOnce(ThrowMySQLError());

    std::map<std::string, std::string> options;
    options["name"] = "foobar";
    options["quiet"] = "1";
    ASSERT_THROW_LIKE(
      config_gen.bootstrap_directory_deployment(dir,
            options, "delme", "delme.key"),
      std::runtime_error,
      "boo!");

    ASSERT_TRUE(mysql_harness::Path(dir).exists());
    ASSERT_TRUE(mysql_harness::Path("delme.key").exists());
  }
  mysql_harness::reset_keyring();

  // don't cleanup on failure in early validation if dir already existed before
  {
    StrictMock<MockMySQLSession> mysql;
    ::testing::InSequence s;

    ConfigGenerator config_gen;
    EXPECT_CALL(mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
    config_gen.init(&mysql);
    mysql.feed_query_row({"mycluter", "myreplicaset", "pm", "somehost:3306"});

    std::map<std::string, std::string> options;
    options["name"] = "force\nfailure";
    options["quiet"] = "1";
    ASSERT_THROW(
      config_gen.bootstrap_directory_deployment(dir,
            options, "delme", "delme.key"),
      std::runtime_error);
    ASSERT_TRUE(mysql_harness::Path(dir).exists());
    ASSERT_TRUE(mysql_harness::Path("delme.key").exists());
  }
  mysql_harness::reset_keyring();
  mysqlrouter::delete_recursive(dir);
  mysqlrouter::delete_file("delme.key");
}


static void bootstrap_overwrite_test(const std::string &dir,
                                     const std::string &name,
                                     bool force,
                                     const char *cluster_name,
                                     bool expect_fail) {
  StrictMock<MockMySQLSession> mysql;
  ::testing::InSequence s;

  ConfigGenerator config_gen;
  EXPECT_CALL(mysql, query_one(_)).WillOnce(Return(new ResultRow({"mysql_innodb_cluster_metadata"})));
  config_gen.init(&mysql);
  if (!expect_fail)
    expect_bootstrap_queries(mysql, cluster_name);
  else
    mysql.feed_query_row({cluster_name, "myreplicaset", "pm", "somehost:3306"});

  std::map<std::string, std::string> options;
  options["name"] = name;
  options["quiet"] = "1";
  if (force)
    options["force"] = "1";
  config_gen.bootstrap_directory_deployment(dir,
      options, "delme", dir+"/delme.key");
}


TEST_F(ConfigGeneratorTest, bootstrap_overwrite) {
  const std::string dir = "configtest";

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

  // same    no          same           OK (refreshing config)
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false));
  mysql_harness::reset_keyring();
  mysqlrouter::delete_recursive(dir);

  // same    no          diff           FAIL
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false));
  mysql_harness::reset_keyring();
  ASSERT_THROW_LIKE(bootstrap_overwrite_test(dir, "myname", false, "kluster", true),
                    std::runtime_error,
                    "If you'd like to replace it, please use the --force");
  mysql_harness::reset_keyring();
  mysqlrouter::delete_recursive(dir);

  // same    yes         same           OK
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", true, "cluster", false));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", true, "cluster", false));
  mysql_harness::reset_keyring();

  // same    yes         diff           OK (replacing config)
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", true, "kluster", false));
  mysql_harness::reset_keyring();
  mysqlrouter::delete_recursive(dir);

  // diff    no          same           OK (refreshing config)
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "xmyname", false, "cluster", false));
  mysql_harness::reset_keyring();
  mysqlrouter::delete_recursive(dir);

  // diff    no          diff           FAIL
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false));
  mysql_harness::reset_keyring();
  ASSERT_THROW_LIKE(bootstrap_overwrite_test(dir, "xmyname", false, "kluster", true),
                    std::runtime_error,
                    "If you'd like to replace it, please use the --force");
  mysql_harness::reset_keyring();
  mysqlrouter::delete_recursive(dir);

  // diff    yes         same           OK
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", true, "cluster", false));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "xmyname", true, "cluster", false));
  mysql_harness::reset_keyring();
  mysqlrouter::delete_recursive(dir);

  // diff    yes         diff           OK (replacing config)
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "myname", false, "cluster", false));
  mysql_harness::reset_keyring();
  ASSERT_NO_THROW(bootstrap_overwrite_test(dir, "xmyname", true, "kluster", false));
  mysql_harness::reset_keyring();
  mysqlrouter::delete_recursive(dir);
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
