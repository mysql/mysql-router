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

#ifndef GROUP_REPLICATION_METADATA_INCLUDED
#define GROUP_REPLICATION_METADATA_INCLUDED

#include <string>
#include <vector>
#include <map>
#include <mysql.h>


struct GroupReplicationMember {
  enum class State {
    Online,
    Recovering,
    Unreachable,
    Offline,
    Other
  };
  enum class Role {
    Primary,
    Secondary,
    Other
  };
  std::string member_id;
  std::string host;
  uint16_t port;
  State state;
  Role role;
};

/** Fetches the list of group replication members known to the instance of the
 * given connection.
 */
std::map<std::string, GroupReplicationMember> fetch_group_replication_members(MYSQL *mysql);

#endif
