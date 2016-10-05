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


class ConfigGeneratorTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) < 0) {
      char buf[100];
#ifndef _WIN32
      strerror_r(errno, buf, sizeof(buf));
#else
      strerror_s(buf, sizeof(buf), errno);
#endif
      throw std::runtime_error(std::string("Unable to get hostname: ") + buf);
    }
#ifndef _WIN32
    struct hostent *hp = gethostbyname2(hostname, AF_INET);
#else
    struct hostent *hp = gethostbyname(hostname);
#endif
    if (!hp) {
      throw std::runtime_error("Unable to get local IP address");
    }
    for (char **p = hp->h_addr_list; *p; ++p) {
      char *ip = inet_ntoa(*(struct in_addr*)*p);
      if (ip) {
        my_ip = ip;
      }
      break;
    }
  }

  std::string my_ip;
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

static void expect_create_account(MockMySQLSession &mock_mysql, const std::string &my_ip) {
  ::testing::InSequence s;
  EXPECT_CALL(mock_mysql, execute("DROP USER IF EXISTS cluster_user@'"+my_ip+"'"));
  EXPECT_CALL(mock_mysql, execute("CREATE USER cluster_user@'"+my_ip+"' IDENTIFIED BY 'secret'"));
  EXPECT_CALL(mock_mysql, execute("GRANT SELECT ON mysql_innodb_cluster_metadata.* TO cluster_user@'"+my_ip+"'"));
  EXPECT_CALL(mock_mysql, execute("GRANT SELECT ON performance_schema.replication_group_members TO cluster_user@'"+my_ip+"'"));
}

TEST_F(ConfigGeneratorTest, create_config_single_master) {
  std::stringstream output;
  MockMySQLSession mock_mysql;

  std::map<std::string, std::string> user_options;

  ConfigGenerator config_gen;
  config_gen.init(&mock_mysql);
  ConfigGenerator::Options options = config_gen.fill_options(false, user_options);

  config_gen.create_config(output,
                      123, "myrouter",
                      "server1,server2,server3",
                      "mycluster",
                      "myreplicaset",
                      "cluster_user",
                      "secret",
                      options);
  ASSERT_THAT(output.str(),
    Eq("# File automatically generated during MySQL Router bootstrap\n"
        "[DEFAULT]\n"
        "\n"
        "[logger]\n"
        "level = INFO\n"
        "\n"
        "[metadata_cache]\n"
        "router_id=123\n"
        "router_tag=myrouter\n"
        "bootstrap_server_addresses=server1,server2,server3\n"
        "user=cluster_user\n"
        "password=secret\n"
        "metadata_cluster=mycluster\n"
        "ttl=300\n"
        "metadata_replicaset=myreplicaset\n"
        "\n"
        "[routing:myreplicaset_rw]\n"
        "bind_port=6446\n"
        "destinations=metadata-cache:///myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "\n"
        "[routing:myreplicaset_ro]\n"
        "bind_port=6447\n"
        "destinations=metadata-cache:///myreplicaset?role=SECONDARY\n"
        "mode=read-only\n\n"));
}


TEST_F(ConfigGeneratorTest, create_config_multi_master) {
  std::stringstream output;
  MockMySQLSession mock_mysql;

  std::map<std::string, std::string> user_options;

  ConfigGenerator config_gen;
  config_gen.init(&mock_mysql);
  ConfigGenerator::Options options = config_gen.fill_options(true, user_options);

  expect_create_account(mock_mysql, my_ip);
  config_gen.create_config(output,
                      123, "myrouter",
                      "server1,server2,server3",
                      "mycluster",
                      "myreplicaset",
                      "cluster_user",
                      "secret",
                      options);
  ASSERT_THAT(output.str(),
    Eq("# File automatically generated during MySQL Router bootstrap\n"
        "[DEFAULT]\n"
        "\n"
        "[logger]\n"
        "level = INFO\n"
        "\n"
        "[metadata_cache]\n"
        "router_id=123\n"
        "router_tag=myrouter\n"
        "bootstrap_server_addresses=server1,server2,server3\n"
        "user=cluster_user\n"
        "password=secret\n"
        "metadata_cluster=mycluster\n"
        "ttl=300\n"
        "metadata_replicaset=myreplicaset\n"
        "\n"
        "[routing:myreplicaset_rw]\n"
        "bind_port=6446\n"
        "destinations=metadata-cache:///myreplicaset?role=PRIMARY\n"
        "mode=read-write\n"
        "\n"));
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
