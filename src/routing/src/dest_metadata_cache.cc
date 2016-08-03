/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include <netdb.h>
#include <netinet/tcp.h>

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"
#include "mysqlrouter/metadata_cache.h"
#include "logger.h"

using mysqlrouter::to_string;
using routing::get_mysql_socket;
using std::out_of_range;
using std::runtime_error;
using std::chrono::duration_cast;
using std::chrono::system_clock;
using std::chrono::seconds;

using metadata_cache::lookup_replicaset;
using metadata_cache::ManagedInstance;

std::vector<TCPAddress> DestMetadataCacheGroup::get_available() {
  auto managed_servers = lookup_replicaset(ha_replicaset).instance_vector;
  std::vector<TCPAddress> available;

  for (auto &it: managed_servers) {
    // Spare and Faulty are not used; skip until next
    if (!(it.role == "scale-out" ||
          it.role == "master")) {
      continue;
    }

    if (routing_mode == "read-only" && it.mode == "read-only") {
      // Secondary read-only
      available.push_back(TCPAddress(it.host, static_cast<uint16_t >(it.port)));
    } else if ((routing_mode == "read-write" &&
                (it.mode == "read-write" ||
                 it.mode == "write-only")) ||
               allow_primary_reads_) {
      // Primary and secondary read-write/write-only
      available.push_back(TCPAddress(it.host, static_cast<uint16_t >(it.port)));
    }
  }

  return available;
}

void DestMetadataCacheGroup::init() {

  auto query_part = uri_query.find("allow_primary_reads");
  if (query_part != uri_query.end()) {
    if (routing_mode == "read-only") {
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

  try {

    auto available = get_available();
    if (available.empty()) {
      return -1;
    }

    auto next_up = current_pos_;
    if (next_up >= available.size()) {
      next_up = 0;
      current_pos_ = 0;
    }

    std::lock_guard<std::mutex> lock(mutex_update_);
    ++current_pos_;
    if (current_pos_ >= available.size()) {
      current_pos_ = 0;
    }
    return get_mysql_socket(available.at(next_up), connect_timeout);
  } catch (std::runtime_error & re) {
    log_error("Failed getting managed servers from the Metadata server : %s",
              re.what());
  }

  *error = errno;
  return -1;
}
