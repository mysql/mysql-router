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

#include "group_replication_metadata.h"
#include "logger.h"
#include "metadata.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

static MYSQL_RES *run_query(MYSQL *mysql, const std::string &query) {
  if (mysql_query(mysql, query.c_str()) != 0) {
    std::ostringstream ss;
    ss << "Query failed: " << query << " with error: " <<
      mysql_error(mysql);
    throw metadata_cache::metadata_error(ss.str());
  }
  MYSQL_RES *result = mysql_store_result(mysql);
  if (!result) {
    std::ostringstream ss;
    ss << "Failed fetching results: " << query << " with error: " <<
      mysql_error(mysql);
    throw metadata_cache::metadata_error(ss.str());
  }
  return result;
}

std::map<std::string, GroupReplicationMember> fetch_group_replication_members(
    MYSQL *mysql) {
  std::map<std::string, GroupReplicationMember> members;
  std::string primary_member;
  MYSQL_RES *result;
  MYSQL_ROW row;

  result = run_query(mysql, "show status like 'group_replication_primary_member'");
  if ((row = mysql_fetch_row(result)) != nullptr) {
    primary_member = row[1] ? row[1] : "";
  }
  mysql_free_result(result);

  result = run_query(mysql,
      "SELECT member_id, member_host, member_port, member_state, @@group_replication_single_primary_mode"
      " FROM performance_schema.replication_group_members"
      " WHERE channel_name = 'group_replication_applier'");
  if (mysql_num_fields(result) != 5) {
    mysql_free_result(result);
    throw metadata_cache::metadata_error("Unexpected resultset from group_replication query");
  }

  while ((row = mysql_fetch_row(result)) != nullptr) {
    const char *member_id = row[0];
    const char *member_host = row[1];
    const char *member_port = row[2];
    const char *member_state = row[3];
    const bool single_master = row[4] && (strcmp(row[4], "1") == 0 || strcmp(row[4], "ON") == 0);
    if (!member_id || !member_host || !member_port || !member_state) {
      mysql_free_result(result);
      throw metadata_cache::metadata_error("Unexpected value in group_replication_metadata query results");
    }
    GroupReplicationMember member;
    member.member_id = member_id;
    member.host = member_host;
    member.port = static_cast<uint16_t>(std::atoi(member_port));
    if (std::strcmp(member_state, "ONLINE") == 0)
      member.state = GroupReplicationMember::State::Online;
    else if (std::strcmp(member_state, "OFFLINE") == 0)
      member.state = GroupReplicationMember::State::Offline;
    else if (std::strcmp(member_state, "UNREACHABLE") == 0)
      member.state = GroupReplicationMember::State::Unreachable;
    else if (std::strcmp(member_state, "RECOVERING") == 0)
      member.state = GroupReplicationMember::State::Recovering;
    else {
      log_info("Unknown state %s in replication_group_members table for %s", member_state, member_id);
      member.state = GroupReplicationMember::State::Other;
    }
    if (primary_member == member.member_id || !single_master)
      member.role = GroupReplicationMember::Role::Primary;
    else
      member.role = GroupReplicationMember::Role::Secondary;
    members[member_id] = member;
  }
  mysql_free_result(result);

  return members;
}
