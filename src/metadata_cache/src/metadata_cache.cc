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

#include "metadata_cache.h"

#include <vector>
#include <memory>

/**
 * Initialize a connection to the MySQL Metadata server.
 *
 * @param bootstrap_servers The servers that store the metadata.
 * @param user The user name used to authenticate to the metadata server.
 * @param password The password used to authenticate to the metadata server.
 * @param metadata_connection_timeout The time after which a connection to the
 *                                  metadata server should timeout.
 * @param connection_attempts The number of times a connection to metadata must
 *                            be attempted, when a connection attempt fails.
 * @param ttl The TTL of the cached data.
 * @param metadata_replicaset The replicaset that servers as the metadata
 *                            HA setup for the topology metadata.
 */
MetadataCache::MetadataCache(
  const std::vector<mysqlrouter::TCPAddress> &bootstrap_servers,
  const std::string &user,
  const std::string &password,
  int connection_timeout,
  int connection_attempts,
  unsigned int ttl,
  const std::string &metadata_replicaset) {
  std::string host_;
  for (auto s : bootstrap_servers) {
     metadata_cache::ManagedInstance *bootstrap_server_instance =
       new metadata_cache::ManagedInstance;
    host_ = (s.addr == "localhost" ? "127.0.0.1" : s.addr);
    bootstrap_server_instance->host = host_;
    bootstrap_server_instance->port = s.port;
    metadata_servers_.push_back(*bootstrap_server_instance);
  }
  ttl_ = ttl;
  metadata_replicaset_ = metadata_replicaset;
  terminate_ = false;
  meta_data_ = get_instance(user, password, connection_timeout,
                            connection_attempts, ttl);
  meta_data_->connect(metadata_servers_);
  refresh();
}

/**
 * Stop the refresh thread.
 */
MetadataCache::~MetadataCache() {
  terminate_ = true;
  if (refresh_thread_.joinable()) {
    refresh_thread_.join();
  }
}

/**
 * Connect to the metadata servers and refresh the metadata information in the
 * cache.
 */
void MetadataCache::start() {
  auto refresh_loop = [this] {
    bool valid_connection_ = false;
    while (!terminate_) {
      {
        std::lock_guard<std::mutex> lock(metadata_servers_mutex_);
        valid_connection_ = meta_data_->connect(metadata_servers_);
      }
      if (valid_connection_) {
        refresh();
      } else {
        meta_data_->disconnect();
      }
      std::this_thread::sleep_for(std::chrono::seconds(ttl_));
    }
  };
  refresh_thread_ = std::thread(refresh_loop);
}

/**
 * Stop the refresh thread.
 */
void MetadataCache::stop() {
  terminate_ = true;
  if (refresh_thread_.joinable()) {
    refresh_thread_.join();
  }
}

/**
 * Return a list of servers that are part of a replicaset.
 *
 * @param replicaset_name The replicaset that is being looked up.
 */
std::vector<metadata_cache::ManagedInstance> MetadataCache::replicaset_lookup(
  const std::string &replicaset_name) {
  std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
  auto replicaset = replicaset_data_.find(replicaset_name);

  if (replicaset == replicaset_data_.end()) {
    log_warning("Replicaset '%s' not available", replicaset_name.c_str());
    return {};
  }
  return replicaset_data_[replicaset_name];
}

/**
 * Refresh the metadata information in the cache.
 */
void MetadataCache::refresh() {
  try {
    // Fetch the metadata and store it in a temporary variable.
    std::map<std::string, std::vector<metadata_cache::ManagedInstance>>
      replicaset_data_temp = meta_data_->fetch_instances();
    {
      // Ensure that the refresh does not result in an inconsistency during the
      // lookup.
      std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
      replicaset_data_ = replicaset_data_temp;
    }

    // Fetch the set of servers in the primary replicaset. These servers
    // store the metadata information.
    std::vector<metadata_cache::ManagedInstance> metadata_servers_temp_ =
      replicaset_lookup(metadata_replicaset_);
    // If the metadata replicaset contains servers, replace the current list
    // of metadata servers with the new list.
    if (!metadata_servers_temp_.empty()) {
      std::lock_guard<std::mutex> lock(metadata_servers_mutex_);
      metadata_servers_ = metadata_servers_temp_;
    }
  } catch (const std::runtime_error &exc) {
    log_debug("Failed fetching data: %s", exc.what());
  }
}
