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

#include "farm_metadata.h"
#include "metadata_cache.h"

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

FarmMetadata::FarmMetadata(const std::string &user, const std::string &password,
                           int connection_timeout, int connection_attempts,
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
FarmMetadata::~FarmMetadata() {
  disconnect();
}

bool FarmMetadata::connect(const std::vector<metadata_cache::ManagedInstance>
                           & metadata_servers) noexcept {
  // It could happen that the server with which a connection existed
  // for fetching the metadata is no longer part of the metadata
  // replicaset. Hence it is safe to take a fresh connection. If we
  // want to reuse connection, we need a mechanism to check if a server
  // is still part of the metadata replicaset.
  unsigned int protocol = MYSQL_PROTOCOL_TCP;
  bool reconnect = false;
  std::string host = "";
  connected_ = false;

  // Iterate through the list of servers in the metadata replicaset
  // to fetch a valid connection using which the metadata can be
  // fetched.
  for (auto& mi : metadata_servers) {
    // Terminate an existing connection to get a clean connection to
    // to a metadata server.
    disconnect();
    assert(metadata_connection_ == nullptr);
    metadata_connection_ = mysql_init(nullptr);
    if (!metadata_connection_) {
      log_error("Failed initializing MySQL client connection");
      return connected_;
    }

    host = (mi.host == "localhost" ? "127.0.0.1" : mi.host);

    // Following would fail only when invalid values are given. It is not possible
    // for the user to change these values.
    mysql_options(metadata_connection_, MYSQL_OPT_CONNECT_TIMEOUT,
                  &connection_timeout_);
    mysql_options(metadata_connection_, MYSQL_OPT_PROTOCOL,
                  reinterpret_cast<char *> (&protocol));
    mysql_options(metadata_connection_, MYSQL_OPT_RECONNECT, &reconnect);

    const unsigned long client_flags = (
      CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 |
      CLIENT_MULTI_RESULTS
      );

    if (mysql_real_connect(metadata_connection_, host.c_str(), user_.c_str(),
                           password_.c_str(), nullptr,
                           static_cast<unsigned int>(mi.port), nullptr,
                           client_flags)) {
      connected_ = true;
      log_info("Connected with metadata server running on %s", host.c_str());
      break;
    } else {
      log_error("Failed connecting with Metadata Server %s:%d: %s",
                host.c_str(), mi.port, mysql_error(metadata_connection_));
      connected_ = false;
    }
  }
  if (!connected_) {
    log_error("Failed connecting with any of the bootstrap servers");
  }
  return connected_;
}

void FarmMetadata::disconnect() noexcept {
  connected_ = false;
  if (metadata_connection_ != nullptr) {
    mysql_close(metadata_connection_);
  }
  metadata_connection_ = nullptr;
}

MYSQL_RES *FarmMetadata::fetch_metadata(const std::string &query) {

  if (!connected_) {
    return nullptr;
  }

  int status = 0;
  MYSQL_RES *result;

  status = mysql_query(metadata_connection_, query.c_str());

  if (status) {
    std::ostringstream ss;
    ss << "Query failed: " << query << "With error: " <<
      mysql_error(metadata_connection_);
    throw metadata_cache::metadata_error(ss.str());
  }

  result = mysql_store_result(metadata_connection_);
  if (result) {
    return result;
  } else {
    std::ostringstream ss;
    ss << "Failed fetching results: " << query << "With error: " <<
      mysql_error(metadata_connection_);
    throw metadata_cache::metadata_error(ss.str());
  }
}

/** @brief Returns relation between replicaset ID and list of servers
 *
 * Returns relation as a std::map between replicaset ID and list of managed servers.
 *
 * @return Map of replicaset ID, server list pairs.
 */
std::map<std::string, std::vector<metadata_cache::ManagedInstance>> FarmMetadata::fetch_instances() {

  // The query fetches the dump of relevant information about the server
  // instances that are part of the managed topology.
  std::string query = "SELECT "
                 "R.replicaset_name, "
                 "I.instance_name, "
                 "I.role, "
                 "I.mode, "
                 "I.weight, "
                 "I.version_token, "
                 "H.location, "
                 "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysql.host')), "
                 "JSON_EXTRACT(I.addresses, '$.mysql.port') "
                 "FROM "
                 "farm_metadata_schema.instances AS I, "
                 "farm_metadata_schema.hosts AS H, "
                 "farm_metadata_schema.replicasets AS R "
                 "WHERE "
                 "I.host_id = H.host_id "
                 "AND "
                 "R.replicaset_id = I.replicaset_id";

  // The following instance map stores a list of servers mapped to every
  // replicaset ID.
  // {
  //   {replicaset_1:[host1:port1, host2:port2, host3:port3]},
  //   {replicaset_2:[host4:port4, host5:port5, host6:port6]},
  //   ...
  //   {replicaset_n:[hostj:portj, hostk:portk, hostl:portl]}
  // }
  std::map<std::string, std::vector<metadata_cache::ManagedInstance>> instance_map;

  MYSQL_ROW row = nullptr;
  MYSQL_RES *result = fetch_metadata(query);

  if (!result) {
    throw metadata_cache::metadata_error("Failed executing " + query);
  } else {
    unsigned int num_fields = mysql_num_fields(result);
    if (num_fields != 9) {
      throw metadata_cache::metadata_error("Unexpected number of fields in"
                                           " the result set: " + std::to_string(num_fields));
    }
  }

  // Deserialize the result set into a map that stores a list of server
  // instance objects mapped to each replicaset.
  while ((row = mysql_fetch_row(result)) != nullptr) {
    metadata_cache::ManagedInstance s;
    s.replicaset_name = get_string(row[0]);
    s.instance_name = get_string(row[1]);
    s.role = get_string(row[2]);
    s.mode = get_string(row[3]);
    s.weight = std::strtof(row[4], nullptr);
    s.version_token = static_cast<unsigned int>(std::atoi(row[5]));
    s.location = get_string(row[6]);
    s.host = get_string(row[7]);
    s.port = static_cast<unsigned int>(std::atoi(row[8]));
    instance_map[s.replicaset_name].push_back(s);
  }

  mysql_free_result(result);

  return instance_map;
}

unsigned int FarmMetadata::fetch_ttl() {
  return ttl_;
}
