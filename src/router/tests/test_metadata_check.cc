/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <cstring>
#include <sstream>

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


static MySQLSessionReplayer &q_schema_version(MySQLSessionReplayer &m) {
  m.expect_query_one("SELECT * FROM mysql_innodb_cluster_metadata.schema_version");
  return m;
}

static MySQLSessionReplayer &q_schema_version(MySQLSessionReplayer &m,
    const char *major, const char *minor, const char *patch = nullptr) {
  m.expect_query_one("SELECT * FROM mysql_innodb_cluster_metadata.schema_version");
  if (!patch)
    m.then_return(2, {{m.string_or_null(major), m.string_or_null(minor)}});
  else
    m.then_return(3, {{m.string_or_null(major), m.string_or_null(minor), m.string_or_null(patch)}});
  return m;
}

static MySQLSessionReplayer &q_metadata_only_our_group(MySQLSessionReplayer &m) {
  m.expect_query_one("SELECT  ((SELECT count(*) FROM mysql_innodb_cluster_metadata.clusters) <= 1  AND (SELECT count(*) FROM mysql_innodb_cluster_metadata.replicasets) <= 1) as has_one_replicaset, (SELECT attributes->>'$.group_replication_group_name' FROM mysql_innodb_cluster_metadata.replicasets)  = @@group_replication_group_name as replicaset_is_ours");
  return m;
}

static MySQLSessionReplayer &q_metadata_only_our_group(MySQLSessionReplayer &m,
    const char *single_cluster, const char *is_our_own_group) {
  m.expect_query_one("SELECT  ((SELECT count(*) FROM mysql_innodb_cluster_metadata.clusters) <= 1  AND (SELECT count(*) FROM mysql_innodb_cluster_metadata.replicasets) <= 1) as has_one_replicaset, (SELECT attributes->>'$.group_replication_group_name' FROM mysql_innodb_cluster_metadata.replicasets)  = @@group_replication_group_name as replicaset_is_ours");
  m.then_return(2, {{m.string_or_null(single_cluster), m.string_or_null(is_our_own_group)}});
  return m;
}

static MySQLSessionReplayer &q_member_state(MySQLSessionReplayer &m) {
  m.expect_query_one("SELECT member_state FROM performance_schema.replication_group_members WHERE member_id = @@server_uuid");
  return m;
}

static MySQLSessionReplayer &q_member_state(MySQLSessionReplayer &m,
    const char *state) {
  m.expect_query_one("SELECT member_state FROM performance_schema.replication_group_members WHERE member_id = @@server_uuid");
  m.then_return(1, {{m.string_or_null(state)}});
  return m;
}

static MySQLSessionReplayer &q_quorum(MySQLSessionReplayer &m) {
  m.expect_query_one("SELECT SUM(IF(member_state = 'ONLINE', 1, 0)) as num_onlines, COUNT(*) as num_total FROM performance_schema.replication_group_members");
  return m;
}

static MySQLSessionReplayer &q_quorum(MySQLSessionReplayer &m,
    const char *num_onlines, const char *num_total) {
  m.expect_query_one("SELECT SUM(IF(member_state = 'ONLINE', 1, 0)) as num_onlines, COUNT(*) as num_total FROM performance_schema.replication_group_members");
  m.then_return(2, {{m.string_or_null(num_onlines), m.string_or_null(num_total)}});
  return m;
}

static MySQLSessionReplayer &q_single_primary_info(MySQLSessionReplayer &m) {
  m.expect_query_one("SELECT @@group_replication_single_primary_mode=1 as single_primary_mode,        (SELECT variable_value FROM performance_schema.global_status WHERE variable_name='group_replication_primary_member') as primary_member,         @@server_uuid as my_uuid");
  return m;
}

static MySQLSessionReplayer &q_single_primary_info(MySQLSessionReplayer &m,
    bool single_primary_mode, const char *primary_uuid, const char *my_uuid) {
  m.expect_query_one("SELECT @@group_replication_single_primary_mode=1 as single_primary_mode,        (SELECT variable_value FROM performance_schema.global_status WHERE variable_name='group_replication_primary_member') as primary_member,         @@server_uuid as my_uuid");
  m.then_return(3, {{m.string_or_null(single_primary_mode ? "1" : "0"), m.string_or_null(primary_uuid), m.string_or_null(my_uuid)}});
  return m;
}

// Unknown database 'mysql_innodb_cluster_metadata' (1049)
TEST(MetadataCheck, metadata_unknown_database) {
  MySQLSessionReplayer m;

  q_schema_version(m).then_error("error", 1049); // unknown database
  ASSERT_THROW_LIKE(
      mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server does not seem to contain metadata for a MySQL InnoDB cluster");
}

// check that the server has the metadata in the correct version
TEST(MetadataCheck, metadata_missing) {
  MySQLSessionReplayer m;

  q_schema_version(m).then_error("error", 1146); // table doesn't exist
  ASSERT_THROW_LIKE(
      mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server does not seem to contain metadata for a MySQL InnoDB cluster");
}

TEST(MetadataCheck, metadata_bad_version) {
  MySQLSessionReplayer m;

  q_schema_version(m, "0", "0", "0");
  ASSERT_THROW_LIKE(
      mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "This version of MySQL Router is not compatible with the provided MySQL InnoDB cluster metadata");

  // unexpected server errors should bubble up to the caller
  m.expect_query_one("SELECT * FROM mysql_innodb_cluster_metadata.schema_version").
      then_error("Access denied for user 'native'@'%' to database 'mysql_innodb_cluster_metadata'", 1044);
  ASSERT_THROW_LIKE(
      mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "Access denied for user 'native'@'%' to database 'mysql_innodb_cluster_metadata'");
}

// check that the server we're querying contains metadata for the group it's in
//   (metadata server group must be same as managed group currently)
TEST(MetadataCheck, metadata_unsupported) {
  MySQLSessionReplayer m;

  q_schema_version(m, "1", "0");
  q_metadata_only_our_group(m, "2", nullptr);
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server contains an unsupported InnoDB cluster metadata.");

  q_schema_version(m, "1", "0");
  q_metadata_only_our_group(m, "0", nullptr);
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server contains an unsupported InnoDB cluster metadata.");

  q_schema_version(m, "1", "0", "0");
  q_metadata_only_our_group(m, "2", nullptr);
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server contains an unsupported InnoDB cluster metadata.");

  q_schema_version(m, "1", "0", "0");
  q_metadata_only_our_group(m, "0", nullptr);
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server contains an unsupported InnoDB cluster metadata.");

  // starting from 1.0.1, group_name in the metadata becomes mandatory
  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "0", "1");
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server contains an unsupported InnoDB cluster metadata.");

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "0", "0");
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server contains an unsupported InnoDB cluster metadata.");

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "0");
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server contains an unsupported InnoDB cluster metadata.");

  // unexpected server errors should bubble up to the caller
  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m).
      then_error("Access denied for user 'native'@'%' to database 'mysql_innodb_cluster_metadata'", 1044);
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "Access denied for user 'native'@'%' to database 'mysql_innodb_cluster_metadata'");
}

// check that the server we're bootstrapping from has GR enabled
TEST(MetadataCheck, metadata_gr_enabled) {
  MySQLSessionReplayer m;

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "OFFLINE");

  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server is currently not an ONLINE member of a InnoDB cluster.");

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "RECOVERING");

  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server is currently not an ONLINE member of a InnoDB cluster.");

  // unexpected server errors should bubble up to the caller
  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m).
      then_error("Access denied for user 'native'@'%' to database 'mysql_innodb_cluster_metadata'", 1044);

  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "Access denied for user 'native'@'%' to database 'mysql_innodb_cluster_metadata'");
}

TEST(MetadataCheck, metadata_gr_enabled_ok) {
  MySQLSessionReplayer m;

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "1", "1");
  q_single_primary_info(m, false, "", "abcd-1234-568");
  ASSERT_NO_THROW(mysqlrouter::check_innodb_metadata_cluster_session(&m, false));
}

// check that the server we're bootstrapping from has quorum
TEST(MetadataCheck, metadata_has_quorum) {
  MySQLSessionReplayer m;

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  // num_onlines, num_total
  q_quorum(m, "1", "3");
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server is currently not in a InnoDB cluster group with quorum and thus may contain inaccurate or outdated data");

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "0", "1");
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server is currently not in a InnoDB cluster group with quorum and thus may contain inaccurate or outdated data.");

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "1", "2");
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server is currently not in a InnoDB cluster group with quorum and thus may contain inaccurate or outdated data.");

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "2", "5");
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server is currently not in a InnoDB cluster group with quorum and thus may contain inaccurate or outdated data.");

  // unexpected server errors should bubble up to the caller
  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m).then_error("Access denied for user 'native'@'%' to database 'mysql_innodb_cluster_metadata'", 1044);
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "Access denied for user 'native'@'%' to database 'mysql_innodb_cluster_metadata'");
}

TEST(MetadataCheck, metadata_has_quorum_ok) {
  MySQLSessionReplayer m;

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "1", "1");
  q_single_primary_info(m, true, "abcd-1234-567", "abcd-1234-567");
  ASSERT_NO_THROW(mysqlrouter::check_innodb_metadata_cluster_session(&m, false));

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "2", "3");
  q_single_primary_info(m, true, "abcd-1234-567", "abcd-1234-567");
  ASSERT_NO_THROW(mysqlrouter::check_innodb_metadata_cluster_session(&m, false));

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "3", "3");
  q_single_primary_info(m, true, "abcd-1234-567", "abcd-1234-567");
  ASSERT_NO_THROW(mysqlrouter::check_innodb_metadata_cluster_session(&m, false));

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "3", "5");
  q_single_primary_info(m, true, "abcd-1234-567", "abcd-1234-567");
  ASSERT_NO_THROW(mysqlrouter::check_innodb_metadata_cluster_session(&m, false));

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "2", "2");
  q_single_primary_info(m, true, "abcd-1234-567", "abcd-1234-567");
  ASSERT_NO_THROW(mysqlrouter::check_innodb_metadata_cluster_session(&m, false));
}

// check that the server we're bootstrapping from is not a non-primary
TEST(MetadataCheck, non_primary) {
  MySQLSessionReplayer m;

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "1", "1");
  // single_primary_mode, primary_member, my_uuid
  q_single_primary_info(m, true, "abcd-1234-567", "abcd-1234-568");
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server is not an updatable member of the cluster. Please try again with the Primary member of the replicaset (abcd-1234-567)");

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "1", "1");
  q_single_primary_info(m, true, "", "abcd-1234-568");
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "The provided server is not an updatable member of the cluster. Please try again with the Primary member of the replicaset");

  // unexpected server errors should bubble up to the caller
  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "1", "1");
  q_single_primary_info(m).then_error("Access denied for user 'native'@'%' to database 'mysql_innodb_cluster_metadata'", 1044);
  ASSERT_THROW_LIKE(mysqlrouter::check_innodb_metadata_cluster_session(&m, false),
      std::runtime_error,
      "Access denied for user 'native'@'%' to database 'mysql_innodb_cluster_metadata'");
}

TEST(MetadataCheck, non_primary_ok) {
  MySQLSessionReplayer m;

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "1", "1");
  // single_primary_mode, primary_member, my_uuid
  q_single_primary_info(m, true, "abcd-1234-567", "abcd-1234-567");
  ASSERT_NO_THROW(mysqlrouter::check_innodb_metadata_cluster_session(&m, false));

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "1", "1");
  q_single_primary_info(m, false, "", "abcd-1234-568");
  ASSERT_NO_THROW(mysqlrouter::check_innodb_metadata_cluster_session(&m, false));

  q_schema_version(m, "1", "0", "1");
  q_metadata_only_our_group(m, "1", "1");
  q_member_state(m, "ONLINE");
  q_quorum(m, "1", "1");
  q_single_primary_info(m, false, "123456789", "abcd-1234-568");
  ASSERT_NO_THROW(mysqlrouter::check_innodb_metadata_cluster_session(&m, false));
}
