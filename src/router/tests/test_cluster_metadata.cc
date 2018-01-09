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

#include "router_test_helpers.h"
#include "mysqlrouter/utils.h"
#include "test/helpers.h"

#include <cstring>

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
#include "mysql_session_replayer.h"
#include "cluster_metadata.h"


using ::testing::Return;
using namespace testing;
using mysqlrouter::MySQLInnoDBClusterMetadata;
using mysqlrouter::HostnameOperationsBase;

class MockHostnameOperations: public HostnameOperationsBase {
 public:
  MOCK_METHOD0(get_my_hostname, std::string());
};


class ClusterMetadataTest : public  ::testing::Test {
protected:
  virtual void SetUp() {
  }

  MySQLSessionReplayer session_replayer;
  MockHostnameOperations hostname_operations;
};

const std::string kQueryGetHostname = "SELECT h.host_id, h.host_name"
            " FROM mysql_innodb_cluster_metadata.routers r"
            " JOIN mysql_innodb_cluster_metadata.hosts h"
            "    ON r.host_id = h.host_id"
            " WHERE r.router_id =";

const std::string kCheckHostExists = "SELECT host_id, host_name, ip_address"
        " FROM mysql_innodb_cluster_metadata.hosts"
        " WHERE host_name =";

const std::string kRegisterRouter = "INSERT INTO mysql_innodb_cluster_metadata.routers"
        "        (host_id, router_name) VALUES";

TEST_F(ClusterMetadataTest, check_router_id_ok) {
  const std::string kHostId = "2";
  const std::string kHostname = "hostname";
  MySQLInnoDBClusterMetadata cluster_metadata(&session_replayer, &hostname_operations);

  session_replayer.expect_query_one(kQueryGetHostname).then_return(2, {{kHostId.c_str(), kHostname.c_str()}});
  EXPECT_CALL(hostname_operations, get_my_hostname()).Times(1).WillOnce(Return(kHostname));

  EXPECT_NO_THROW(cluster_metadata.check_router_id(1));
}

ACTION(ThrowRuntimeException)
{
  throw std::runtime_error("");
}

TEST_F(ClusterMetadataTest, check_router_id_get_hostname_throws) {
  const std::string kHostId = "2";
  const std::string kHostname = "";
  MySQLInnoDBClusterMetadata cluster_metadata(&session_replayer, &hostname_operations);

  session_replayer.expect_query_one(kQueryGetHostname).then_return(2, {{kHostId.c_str(), kHostname.c_str()}});
  EXPECT_CALL(hostname_operations, get_my_hostname()).Times(1).WillOnce(ThrowRuntimeException());


  // get_my_hostname() throwing should be handled inside check_router_id
  EXPECT_NO_THROW(cluster_metadata.check_router_id(1));
}

TEST_F(ClusterMetadataTest, check_router_id_router_not_found) {
  MySQLInnoDBClusterMetadata cluster_metadata(&session_replayer, &hostname_operations);

  session_replayer.expect_query_one(kQueryGetHostname).then_return(2, {});

  try {
    cluster_metadata.check_router_id(1);
    FAIL() << "Expected exception";
  }
  catch (std::runtime_error &e) {
    ASSERT_STREQ("router_id 1 not found in metadata", e.what());
  }
}

TEST_F(ClusterMetadataTest, check_router_id_different_hostname) {
  const std::string kHostId = "2";
  const std::string kHostname1 = "hostname";
  const std::string kHostname2 = "another.hostname";
  MySQLInnoDBClusterMetadata cluster_metadata(&session_replayer, &hostname_operations);

  session_replayer.expect_query_one(kQueryGetHostname).then_return(2, {{kHostId.c_str(), kHostname1.c_str()}});
  EXPECT_CALL(hostname_operations, get_my_hostname()).Times(1).WillOnce(Return(kHostname2));

  try {
    cluster_metadata.check_router_id(1);
    FAIL() << "Expected exception";
  }
  catch (std::runtime_error &e) {
    ASSERT_STREQ("router_id 1 is associated with a different host ('hostname' vs 'another.hostname')", e.what());
  }
}

TEST_F(ClusterMetadataTest, register_router_ok) {
  const std::string kRouterName = "routername";
  const std::string kHostName = "hostname";
  MySQLInnoDBClusterMetadata cluster_metadata(&session_replayer, &hostname_operations);

  session_replayer.expect_query_one(kCheckHostExists).then_return(3, {{"1", kHostName.c_str(), "127.0.0.1"}});
  session_replayer.expect_execute(kRegisterRouter).then_ok();
  EXPECT_CALL(hostname_operations, get_my_hostname()).Times(1).WillOnce(Return(kHostName.c_str()));

  EXPECT_NO_THROW(cluster_metadata.register_router(kRouterName, false));
}

TEST_F(ClusterMetadataTest, register_router_get_hostname_throws) {
  const std::string kRouterName = "routername";
  const std::string kHostName = "";
  MySQLInnoDBClusterMetadata cluster_metadata(&session_replayer, &hostname_operations);

  session_replayer.expect_query_one(kCheckHostExists).then_return(3, {{"1", kHostName.c_str(), "127.0.0.1"}});
  session_replayer.expect_execute(kRegisterRouter).then_ok();
  EXPECT_CALL(hostname_operations, get_my_hostname()).Times(1).WillOnce(ThrowRuntimeException());

  // get_my_hostname() throwing should be handled inside register_router
  EXPECT_NO_THROW(cluster_metadata.register_router(kRouterName, false));
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
