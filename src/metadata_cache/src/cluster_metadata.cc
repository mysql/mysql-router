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

#include "cluster_metadata.h"
//#include "metadata_cache.h"
#include "group_replication_metadata.h"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mysql.h>
#include <mysqlrouter/datatypes.h>
#include "mysqlrouter/uri.h"
#include <errmsg.h>
#include "logger.h"


/**
 * Return a string representation of the input character string.
 *
 * @param input_str A character string.
 *
 * @return A string object encapsulation of the input character string. An empty
 *         string if input string is nullptr.
 */
std::string get_string(const char *input_str) {
  if (input_str == nullptr) {
    return "";
  }
  return std::string(input_str);
}

ClusterMetadata::ClusterMetadata(const std::string &user,
                                 const std::string &password,
                                 int connection_timeout,
                                 int connection_attempts,
                                 unsigned int ttl) {
  this->metadata_connection_ = nullptr;
  this->metadata_uuid_ = "";
  this->ttl_ = ttl;
  this->message_ = "";
  this->user_ = user;
  this->password_ = password;
  this->connection_timeout_ = connection_timeout;
  this->connection_attempts_ = connection_attempts;
  this->reconnect_tries_ = 0;
}

/** @brief Destructor
 *
 * Disconnect and release the connection to the metadata node.
 */
ClusterMetadata::~ClusterMetadata() {
  disconnect();
}

bool ClusterMetadata::do_connect(MYSQL *mysql, const metadata_cache::ManagedInstance &mi) {
  unsigned int protocol = MYSQL_PROTOCOL_TCP;
  bool reconnect = false;
  std::string host = "";
  connected_ = false;

  host = (mi.host == "localhost" ? "127.0.0.1" : mi.host);

  // Following would fail only when invalid values are given. It is not possible
  // for the user to change these values.
  mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT,
                &connection_timeout_);
  mysql_options(mysql, MYSQL_OPT_PROTOCOL,
                reinterpret_cast<char *> (&protocol));
  mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);

  const unsigned long client_flags = (
    CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 |
    CLIENT_MULTI_RESULTS
    );
  if (mysql_real_connect(mysql, host.c_str(), user_.c_str(),
                         password_.c_str(), nullptr,
                         static_cast<unsigned int>(mi.port), nullptr,
                         client_flags)) {
    log_info("Connected with metadata server running on %s:%i", host.c_str(), mi.port);
    return true;
  } else {
    log_error("Failed connecting with Metadata Server %s:%d: %s",
              host.c_str(), mi.port, mysql_error(mysql));
  }
  return false;
}

bool ClusterMetadata::connect(const std::vector<metadata_cache::ManagedInstance>
                           & metadata_servers) noexcept {
  // It could happen that the server with which a connection existed
  // for fetching the metadata is no longer part of the metadata
  // replicaset. Hence it is safe to take a fresh connection. If we
  // want to reuse connection, we need a mechanism to check if a server
  // is still part of the metadata replicaset.
  connected_ = false;

  // Terminate an existing connection to get a clean connection to
  // to a metadata server.
  disconnect();
  assert(metadata_connection_ == nullptr);
  metadata_connection_ = mysql_init(nullptr);
  if (!metadata_connection_) {
    log_error("Failed initializing MySQL client connection");
    return connected_;
  }

  // Iterate through the list of servers in the metadata replicaset
  // to fetch a valid connection using which the metadata can be
  // fetched.
  for (auto& mi : metadata_servers) {
    if (do_connect(metadata_connection_, mi)) {
      metadata_connection_address_ = (mi.host == "localhost" ? "127.0.0.1" : mi.host) + ":" + std::to_string(mi.port);
      connected_ = true;
      break;
    } else {
      connected_ = false;
    }
  }
  if (!connected_) {
    log_error("Failed connecting with any of the bootstrap servers");
  }
  return connected_;
}

void ClusterMetadata::disconnect() noexcept {
  connected_ = false;
  if (metadata_connection_ != nullptr) {
    mysql_close(metadata_connection_);
  }
  metadata_connection_ = nullptr;
  metadata_connection_address_.clear();
}

MYSQL_RES *ClusterMetadata::run_query(const std::string &query) {

  if (!connected_) {
    log_warning("run_query() called while not connected");
    return nullptr;
  }

  int status = 0;
  MYSQL_RES *result;

  status = mysql_real_query(metadata_connection_, query.c_str(), query.length());

  if (status) {
    std::ostringstream ss;
    ss << mysql_error(metadata_connection_) << ": " << query;
    throw metadata_cache::metadata_error(ss.str());
  }

  result = mysql_store_result(metadata_connection_);
  if (result) {
    return result;
  } else {
    std::ostringstream ss;
    ss << mysql_error(metadata_connection_) << ": fetching results for: " << query;
    throw metadata_cache::metadata_error(ss.str());
  }
}

void ClusterMetadata::update_replicaset_status(const std::string &name,
    std::vector<metadata_cache::ManagedInstance> &instances) {
  bool reuse_connection = false;
  // as an optimization, check if the instance we're connected to is part of this replicaset
  if (connected_) {
    for (auto &&mi : instances) {
      std::string addr = (mi.host == "localhost" ? "127.0.0.1" : mi.host) + ":" + std::to_string(mi.port);
      if (addr == metadata_connection_address_) {
        reuse_connection = true;
        break;
      }
    }
  }
  MYSQL *conn = nullptr;
  if (reuse_connection)
    conn = metadata_connection_;
  else {
    // connect to someone in the replicaset
    MYSQL *tmp = mysql_init(NULL);
    if (!tmp)
      throw metadata_cache::metadata_error("Error initializing MySQL connection");
    for (auto &&mi : instances) {
      if (do_connect(tmp, mi)) {
        conn = tmp;
        break;
      }
    }
  }
  if (!conn) {
    throw metadata_cache::metadata_error(
        "Could not establish a connection to replicaset "+name);
  }
  try
  {
    std::map<std::string, GroupReplicationMember> member_status(fetch_group_replication_members(conn));
    if (!reuse_connection) {
      mysql_close(conn);
    }
    log_debug("Replicaset '%s' has %i members in metadata, %i in status table",
      name.c_str(), instances.size(), member_status.size());
    check_replicaset_status(instances, member_status);
  } catch (...) {
    log_warning("Unable to fetch live group_replication member data replicaset %s",
      name.c_str());
    if (!reuse_connection) {
      mysql_close(conn);
    }
    throw;
  }
}

metadata_cache::ReplicasetStatus ClusterMetadata::check_replicaset_status(
    std::vector<metadata_cache::ManagedInstance> &instances,
    std::map<std::string, GroupReplicationMember> &member_status) {
  // check if all members are visible as online/there is quorum
  int online_count = 0;
  int unreachable_count = 0;
  int recovering_count = 0;
  std::string primary_instance;
  for (auto &member : instances) {
    auto status = member_status.find(member.mysql_server_uuid);
    if (status != member_status.end()) {
      if (status->second.role == GroupReplicationMember::Role::Primary) {
        primary_instance = member.mysql_server_uuid;
        member.mode = metadata_cache::ServerMode::ReadWrite;
      } else {
        member.mode = metadata_cache::ServerMode::ReadOnly;
      }
      switch (status->second.state) {
        case GroupReplicationMember::State::Online:
          online_count++;
          break;
        case GroupReplicationMember::State::Recovering:
          recovering_count++;
          member.mode = metadata_cache::ServerMode::Unavailable;
          break;
        case GroupReplicationMember::State::Unreachable:
          unreachable_count++;
          member.mode = metadata_cache::ServerMode::Unavailable;
          break;
        case GroupReplicationMember::State::Offline:
        case GroupReplicationMember::State::Other:
          member.mode = metadata_cache::ServerMode::Unavailable;
          break;
      }
    } else {
      member.mode = metadata_cache::ServerMode::Unavailable;
      log_warning("Member %s defined in metadata not found in actual replicaset", member.mysql_server_uuid.c_str());
    }
  }

  metadata_cache::ReplicasetStatus rs_status = metadata_cache::ReplicasetStatus::Unavailable;
  // everyone online
  if (online_count > 0 && unreachable_count == 0) {
    // check if there are enough members to form quorum
    // trying to write to a group with no quorum will block everything
    if (online_count < 2) {
      rs_status = metadata_cache::ReplicasetStatus::AvailableReadOnly;
    } else {
      rs_status = metadata_cache::ReplicasetStatus::AvailableWritable;
    }
  } else if (unreachable_count > 0) {
    // if there are members that are unreachable from the one we're
    // connected to, we could be in a partitioning scenario
    // ideally we try to connect to all instances to see if there's
    // quorum in any of them, for now we just continue with the
    // instances marked as unusable
    if (online_count == 0)
      rs_status = metadata_cache::ReplicasetStatus::Unavailable;
    else
      rs_status = metadata_cache::ReplicasetStatus::Partitioned;
  }
  return rs_status;
}

ClusterMetadata::InstancesByReplicaSet ClusterMetadata::fetch_instances(
    const std::string &cluster_name) {
  log_debug("Updating metadata information for cluster '%s'", cluster_name.c_str());
  // fetch existing replicasets in the cluster from the metadata server
  InstancesByReplicaSet rs_instances(fetch_instances_from_metadata_server(cluster_name));

  // now connect to each replicaset and query them for the list and status of their members
  for (auto &&rs : rs_instances) {
    update_replicaset_status(rs.first, rs.second);
  }

  if (rs_instances.empty())
    log_warning("No replicasets defined for cluster '%s'", cluster_name.c_str());

  return rs_instances;
}

ClusterMetadata::InstancesByReplicaSet ClusterMetadata::fetch_instances_from_metadata_server(const std::string &cluster_name) {
  std::string cluster_name_escaped;
  cluster_name_escaped.resize((cluster_name.size()+1)*2+1);
  {
    size_t length = mysql_real_escape_string_quote(metadata_connection_,
        &cluster_name_escaped[0], cluster_name.c_str(), cluster_name.size(), '\'');
    cluster_name_escaped.resize(length);
  }
  // The query fetches the dump of relevant information about the server
  // instances that are part of the managed topology.
  std::string query("SELECT "
                    "R.replicaset_name, "
                    "I.mysql_server_uuid, "
                    "I.role, "
                    "I.weight, "
                    "I.version_token, "
                    "H.location, "
                    "I.addresses->>'$.mysqlClassic', "
                    "I.addresses->>'$.mysqlX' "
                    "FROM "
                    "mysql_innodb_cluster_metadata.clusters AS F "
                    "JOIN mysql_innodb_cluster_metadata.replicasets AS R "
                    "ON F.cluster_id = R.cluster_id "
                    "JOIN mysql_innodb_cluster_metadata.instances AS I "
                    "ON R.replicaset_id = I.replicaset_id "
                    "JOIN mysql_innodb_cluster_metadata.hosts AS H "
                    "ON I.host_id = H.host_id "
                    "WHERE F.cluster_name = '"+cluster_name_escaped+"';");

  // The following instance map stores a list of servers mapped to every
  // replicaset name.
  // {
  //   {replicaset_1:[host1:port1, host2:port2, host3:port3]},
  //   {replicaset_2:[host4:port4, host5:port5, host6:port6]},
  //   ...
  //   {replicaset_n:[hostj:portj, hostk:portk, hostl:portl]}
  // }
  InstancesByReplicaSet instance_map;

  MYSQL_ROW row = nullptr;
  MYSQL_RES *result = run_query(query);

  if (!result) {
    throw metadata_cache::metadata_error("Failed executing " + query);
  } else {
    unsigned int num_fields = mysql_num_fields(result);
    if (num_fields != 8) {
      throw metadata_cache::metadata_error("Unexpected number of fields in"
                                           " the result set: " + std::to_string(num_fields));
    }
  }
  // Deserialize the result set into a map that stores a list of server
  // instance objects mapped to each replicaset.
  while ((row = mysql_fetch_row(result)) != nullptr) {
    metadata_cache::ManagedInstance s;
    s.replicaset_name = get_string(row[0]);
    s.mysql_server_uuid = get_string(row[1]);
    s.role = get_string(row[2]);
    s.weight = row[3] ? std::strtof(row[3], nullptr) : 0;
    s.version_token = row[4] ? static_cast<unsigned int>(std::atoi(row[4])) : 0;
    s.location = get_string(row[5]);
    try {
      std::string uri = get_string(row[6]);
      std::string::size_type p;
      if ((p = uri.find(':')) != std::string::npos) {
        s.host = uri.substr(0, p);
        s.port = static_cast<unsigned int>(std::atoi(uri.substr(p+1).c_str()));
      } else {
        s.host = uri;
        s.port = 3306;
      }
    } catch (std::runtime_error &e) {
      log_warning("Error parsing URI in metadata for instance %s: '%s': %s",
          row[1], row[6], e.what());
      continue;
    }
    // X protocol support is not mandatory
    if (row[7] && *row[7]) {
      try {
        std::string uri = get_string(row[7]);
        std::string::size_type p;
        if ((p = uri.find(':')) != std::string::npos) {
          s.host = uri.substr(0, p);
          s.xport = static_cast<unsigned int>(std::atoi(uri.substr(p+1).c_str()));
        } else {
          s.host = uri;
          s.xport = 33060;
        }
      } catch (std::runtime_error &e) {
        log_warning("Error parsing URI in metadata for instance %s: '%s': %s",
            row[1], row[7], e.what());
        continue;
      }
    } else {
      s.xport = s.port * 10;
    }
    instance_map[s.replicaset_name].push_back(s);
  }
  mysql_free_result(result);
  return instance_map;
}

unsigned int ClusterMetadata::fetch_ttl() {
  return ttl_;
}
