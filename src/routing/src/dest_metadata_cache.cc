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

#include "dest_metadata_cache.h"
#include "utils.h"
#include "mysqlrouter/routing.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#ifndef _WIN32
#  include <netdb.h>
#  include <netinet/tcp.h>
#endif

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"
#include "mysqlrouter/metadata_cache.h"
#include "logger.h"

using mysqlrouter::to_string;
using std::out_of_range;
using std::runtime_error;
using std::chrono::duration_cast;
using std::chrono::system_clock;
using std::chrono::seconds;

using metadata_cache::lookup_replicaset;
using metadata_cache::ManagedInstance;

// if client wants a primary and there's none, we can wait up to this amount of
// seconds until giving up and disconnecting the client
// TODO: possibly this should be made into a configurable option
static const int kPrimaryFailoverTimeout = 10;


DestMetadataCacheGroup::DestMetadataCacheGroup(const std::string &metadata_cache, const std::string &replicaset,
  const std::string &mode, const mysqlrouter::URIQuery &query,
  const Protocol::Type protocol) :
    RouteDestination(protocol),
    cache_name_(metadata_cache),
    ha_replicaset_(replicaset),
    uri_query_(query),
    allow_primary_reads_(false),
    current_pos_(0) {
  if (mode == "read-only")
    routing_mode_ = ReadOnly;
  else if (mode == "read-write")
    routing_mode_ = ReadWrite;
  else
    throw std::runtime_error("Invalid routing mode value '"+mode+"'");
  init();
}

std::vector<mysqlrouter::TCPAddress> DestMetadataCacheGroup::get_available(std::vector<std::string> *server_ids) {
  auto managed_servers = lookup_replicaset(ha_replicaset_).instance_vector;
  std::vector<mysqlrouter::TCPAddress> available;
  for (auto &it: managed_servers) {
    if (!(it.role == "HA")) {
      continue;
    }
    auto port = (protocol_ == Protocol::Type::kXProtocol) ? static_cast<uint16_t>(it.xport) : static_cast<uint16_t>(it.port);
    if (routing_mode_ == RoutingMode::ReadOnly && it.mode == metadata_cache::ServerMode::ReadOnly) {
      // Secondary read-only
      available.push_back(mysqlrouter::TCPAddress(it.host, port));
      if (server_ids)
        server_ids->push_back(it.mysql_server_uuid);
    } else if ((routing_mode_ == RoutingMode::ReadWrite &&
                it.mode == metadata_cache::ServerMode::ReadWrite) ||
               allow_primary_reads_) {
      // Primary and secondary read-write/write-only
      available.push_back(mysqlrouter::TCPAddress(it.host, port));
      if (server_ids)
        server_ids->push_back(it.mysql_server_uuid);
    }
  }

  return available;
}

void DestMetadataCacheGroup::init() {

  auto query_part = uri_query_.find("allow_primary_reads");
  if (query_part != uri_query_.end()) {
    if (routing_mode_ == RoutingMode::ReadOnly) {
      auto value = query_part->second;
      std::transform(value.begin(), value.end(), value.begin(), ::tolower);
      if (value == "yes") {
        allow_primary_reads_ = true;
      }
    } else {
      log_warning("allow_primary_reads only works with read-only mode");
    }
  }
}

int DestMetadataCacheGroup::get_server_socket(int connect_timeout, int *error) noexcept {
  while (true) {
    try {
      std::vector<std::string> server_ids;
      auto available = get_available(&server_ids);
      if (available.empty()) {
        log_warning("No available %s servers found for '%s'",
            routing_mode_ == RoutingMode::ReadWrite ? "RW" : "RO",
            ha_replicaset_.c_str());
        return -1;
      }

      size_t next_up = 0;
      {
        std::lock_guard<std::mutex> lock(mutex_update_);
        // round-robin between available nodes
        next_up = current_pos_;
        if (next_up >= available.size()) {
          next_up = 0;
          current_pos_ = 0;
        }
        ++current_pos_;
        if (current_pos_ >= available.size()) {
          current_pos_ = 0;
        }
      }

      int fd = get_mysql_socket(available.at(next_up), connect_timeout);
      if (fd < 0) {
        // Signal that we can't connect to the instance
        metadata_cache::mark_instance_reachability(server_ids.at(next_up),
            metadata_cache::InstanceStatus::Unreachable);
        // if we're looking for a primary member, wait for there to be at least one
        if (routing_mode_ == RoutingMode::ReadWrite &&
            metadata_cache::wait_primary_failover(ha_replicaset_,
                kPrimaryFailoverTimeout)) {
          log_info("Retrying connection for '%s' after possible failover",
                   ha_replicaset_.c_str());
          continue; // retry
        }
      }
      return fd;
    } catch (std::runtime_error & re) {
      log_error("Failed getting managed servers from the Metadata server: %s",
                re.what());
      break;
    }
  }

  *error = errno;
  return -1;
}
