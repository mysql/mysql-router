/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "dest_fabric_cache.h"
#include "utils.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <netdb.h>
#include <netinet/tcp.h>

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"
#include "mysqlrouter/fabric_cache.h"
#include "logger.h"

using mysqlrouter::to_string;
using std::out_of_range;
using std::runtime_error;
using std::chrono::duration_cast;
using std::chrono::system_clock;
using std::chrono::seconds;

using fabric_cache::lookup_group;
using fabric_cache::ManagedServer;

const int kPopulateErrorReportInterval = 10;

void DestFabricCacheGroup::populate_destinations() {

  ++ populate_attempts_;
  try {
    if (!fabric_cache::have_cache(cache_name)) {
      // Cache not active
      if (populate_attempts_ % kPopulateErrorReportInterval == 0 || populate_attempts_ == 1) {
        log_error("Failed populating from Fabric; will retry");
        populate_attempts_ = 10;  // reset, but make sure this is not 1 otherwise we get double message
      }
      return;
    }
    populate_attempts_ = 0;

    auto managed_servers = lookup_group(cache_name, ha_group).server_list;
    clear();

    std::lock_guard<std::mutex> lock(mutex_update_);
    int c_primary = 0;
    int c_secondary = 0;
    for (auto &it: managed_servers) {
      auto server_status = static_cast<ManagedServer::Status>(it.status);
      auto server_mode = static_cast<ManagedServer::Mode>(it.mode);

      // Spare and Faulty are not used
      if (!(server_status == ManagedServer::Status::kPrimary ||
            server_status == ManagedServer::Status::kSecondary)) {
        continue;
      }

      if (server_status == ManagedServer::Status::kPrimary) {
        ++c_primary;
      } else {
        ++c_secondary;
      }

      if (routing_mode == routing::AccessMode::kReadOnly && server_mode == ManagedServer::Mode::kReadOnly) {
        // Secondary read-only
        destinations_.push_back(TCPAddress(it.host, static_cast<uint16_t >(it.port)));
        continue;
      } else if ((routing_mode == routing::AccessMode::kReadWrite &&
                  (server_mode == ManagedServer::Mode::kReadWrite ||
                   server_mode == ManagedServer::Mode::kWriteOnly)) ||
                 allow_primary_reads_) {
        // Primary and secondary read-write/write-only
        destinations_.push_back(TCPAddress(it.host, static_cast<uint16_t >(it.port)));
        continue;
      }
    }
    destination_iter_ = destinations_.begin();
    if (count_primary_ != c_primary || count_secondary_ != c_secondary) {
      log_debug("Got %d managed servers in group '%s'; can use %d", managed_servers.size(),
                ha_group.c_str(), destinations_.size());
    }
    count_primary_ = c_primary;
    count_secondary_ = c_secondary;
  } catch (const fabric_cache::base_error &exc) {
    log_error("Failed populating destinations: %s", exc.what());
  } catch (...) {
    try {
      auto exc_ptr = std::current_exception();
      if (exc_ptr) {
        std::rethrow_exception(exc_ptr);
      }
    } catch (const std::exception &exc) {
      log_error(exc.what());
    }
  }
}

void DestFabricCacheGroup::init() {
  populate_attempts_ = 0;
  auto query_part = uri_query.find("allow_primary_reads");
  if (query_part != uri_query.end()) {
    auto value = query_part->second;
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    if (value == "yes") {
      allow_primary_reads_ = true;
    }
  }

  // Initial population
  while (!destinations_.size()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    populate_destinations();
  }
}

TCPAddress DestFabricCacheGroup::get_server() noexcept {

  populate_destinations();

  if (destinations_.empty()) {
    log_warning("Currently no servers avaialble from Fabric");
    return TCPAddress();
  }

  if (destinations_.size() == 1) {
    return destinations_.at(0);
  }

  std::lock_guard<std::mutex> lock(mutex_update_);
  if (current_pos_ >= destinations_.size()) {
    current_pos_ = 0;
  }
  auto addr = destinations_.at(current_pos_);
  ++current_pos_;
  return addr;
}
