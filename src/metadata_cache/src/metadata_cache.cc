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

#include "common.h"
#include "metadata_cache.h"

#include <cassert>
#include <vector>
#include <memory>
#include <cmath>  // fabs()

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
 * @param cluster The name of the desired cluster in the metadata server
 */
MetadataCache::MetadataCache(
  const std::vector<mysqlrouter::TCPAddress> &bootstrap_servers,
  std::shared_ptr<MetaData> cluster_metadata,
  unsigned int ttl,
  const std::string &cluster) {
  std::string host;
  for (auto s : bootstrap_servers) {
    metadata_cache::ManagedInstance bootstrap_server_instance;
    host = (s.addr == "localhost" ? "127.0.0.1" : s.addr);
    bootstrap_server_instance.host = host;
    bootstrap_server_instance.port = s.port;
    metadata_servers_.push_back(bootstrap_server_instance);
  }
  ttl_ = ttl;
  cluster_name_ = cluster;
  terminate_ = false;
  meta_data_ = cluster_metadata;
  refresh();
}

/**
 * Stop the refresh thread.
 */
MetadataCache::~MetadataCache() {
  stop();
}

/**
 * Connect to the metadata servers and refresh the metadata information in the
 * cache.
 */
void MetadataCache::start() {
  auto refresh_loop = [this] {
    mysql_harness::rename_thread("MDC Refresh");

    while (!terminate_) {
      refresh();

      // wait for up to TTL until next refresh, unless some replicaset
      // loses the primary server.. in that case, we refresh every 1s
      // until we detect a new one was elected
      unsigned int seconds_waited = 0;
      while (seconds_waited < ttl_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        seconds_waited++;
        {
          std::lock_guard<std::mutex> lock(lost_primary_replicasets_mutex_);
          if (!lost_primary_replicasets_.empty())
            break;
        }
      }
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
  return replicaset_data_[replicaset_name].members;
}

bool metadata_cache::ManagedInstance::operator==(const ManagedInstance& other) const {
  return mysql_server_uuid == other.mysql_server_uuid &&
         replicaset_name == other.replicaset_name &&
         role == other.role &&
         mode == other.mode &&
         std::fabs(weight - other.weight) < 0.001 &&  // 0.001 = reasonable guess, change if needed
         host == other.host &&
         location == other.location &&
         port == other.port &&
         version_token == other.version_token &&
         xport == other.xport;
}

inline bool compare_instance_lists(const MetaData::ReplicaSetsByName &map_a,
                                   const MetaData::ReplicaSetsByName &map_b) {
  if (map_a.size() != map_b.size())
    return false;
  auto ai = map_a.begin();
  auto bi = map_b.begin();
  for (; ai != map_a.end(); ++ai, ++bi) {
    if ((ai->first != bi->first) || (ai->second.members.size() != bi->second.members.size()))
      return false;
    auto a = ai->second.members.begin();
    auto b = bi->second.members.begin();
    for (; a != ai->second.members.end(); ++a, ++b) {
      if (!(*a == *b))
        return false;
    }
  }
  return true;
}

static const char *str_mode(metadata_cache::ServerMode mode) {
  switch (mode) {
    case metadata_cache::ServerMode::ReadWrite: return "RW";
    case metadata_cache::ServerMode::ReadOnly: return "RO";
    case metadata_cache::ServerMode::Unavailable: return "n/a";
    default: return "?";
  }
}

/**
 * Refresh the metadata information in the cache.
 */
void MetadataCache::refresh() {

  {
    #if 0 // not used anywhere else so far
    std::lock_guard<std::mutex> lock(metadata_servers_mutex_);
    #endif
    // TODO: connect() could really be called from inside of metadata_->fetch_instances()
    if (!meta_data_->connect(metadata_servers_)) { // metadata_servers_ come from config file
      log_error("Failed connecting to metadata servers");
      return;
    }
  }

  try {
    // Fetch the metadata and store it in a temporary variable.
    std::map<std::string, metadata_cache::ManagedReplicaSet>
      replicaset_data_temp = meta_data_->fetch_instances(cluster_name_);
    bool changed = false;

    {
      // Ensure that the refresh does not result in an inconsistency during the
      // lookup.
      std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
      if (!compare_instance_lists(replicaset_data_, replicaset_data_temp)) {
        replicaset_data_ = replicaset_data_temp;
        changed = true;
      }
    }

    if (changed) {
      log_info("Changes detected in cluster '%s' after metadata refresh",
          cluster_name_.c_str());
      // dump some informational/debugging information about the replicasets
      if (replicaset_data_.empty())
        log_error("Metadata for cluster '%s' is empty!", cluster_name_.c_str());
      else {
        log_info("Metadata for cluster '%s' has %i replicasets:",
          cluster_name_.c_str(), (int)replicaset_data_.size());
        for (auto &rs : replicaset_data_) {
          log_info("'%s' (%i members, %s)", rs.first.c_str(),
                    (int)rs.second.members.size(),
                    rs.second.single_primary_mode ? "single-master" : "multi-master");
          for (auto &mi : rs.second.members) {
            log_info("    %s:%i / %i - role=%s mode=%s", mi.host.c_str(),
                mi.port, mi.xport, mi.role.c_str(), str_mode(mi.mode));

            if (mi.mode == metadata_cache::ServerMode::ReadWrite) {
              // If we were running without a primary and a new one was elected
              // disable the frequent update mode
              std::lock_guard<std::mutex> lock(lost_primary_replicasets_mutex_);
              auto lost_primary = lost_primary_replicasets_.find(rs.first);
              if (lost_primary != lost_primary_replicasets_.end()) {
                log_info("Replicaset '%s' has a new Primary %s:%i [%s].",
                         rs.first.c_str(),
                         mi.host.c_str(), mi.port,
                         mi.mysql_server_uuid.c_str());
                lost_primary_replicasets_.erase(lost_primary);
              }
            }
          }
        }
      }
    }

    /* Not sure about this, the metadata server could be stored elsewhere

    // Fetch the set of servers in the primary replicaset. These servers
    // store the metadata information.
    std::vector<metadata_cache::ManagedInstance> metadata_servers_temp_ =
      replicaset_lookup(cluster_name_);
    // If the metadata replicaset contains servers, replace the current list
    // of metadata servers with the new list.
    if (!metadata_servers_temp_.empty()) {
      std::lock_guard<std::mutex> lock(metadata_servers_mutex_);
      metadata_servers_ = metadata_servers_temp_;
    }*/
  } catch (const std::runtime_error &exc) {
    log_error("Failed fetching metadata: %s", exc.what());
  }
}

void MetadataCache::mark_instance_reachability(const std::string &instance_id,
                                metadata_cache::InstanceStatus status) {
  // If the status is that the primary instance is physically unreachable,
  // we temporarily increase the refresh rate to 1/s until the replicaset
  // is back to having a primary instance.
  std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
  // the replicaset that the given instance belongs to
  metadata_cache::ManagedInstance *instance = nullptr;
  metadata_cache::ManagedReplicaSet *replicaset = nullptr;
  for (auto &rs : replicaset_data_) {
    for (auto &inst : rs.second.members) {
      if (inst.mysql_server_uuid == instance_id) {
        instance = &inst;
        replicaset = &rs.second;
        break;
      }
    }
    if (replicaset)
      break;
  }

  // We only care about loss of primary for the purpose of triggering
  // faster refreshes if we're in single primary mode
  if (replicaset && replicaset->single_primary_mode) {
    std::lock_guard<std::mutex> lplock(lost_primary_replicasets_mutex_);
    switch (status) {
      case metadata_cache::InstanceStatus::Reachable:
        break;
      case metadata_cache::InstanceStatus::InvalidHost:
        log_warning("Primary instance '%s:%i' [%s] of replicaset '%s' is invalid. Increasing metadata cache refresh frequency.",
                    instance->host.c_str(), instance->port, instance_id.c_str(),
                    replicaset->name.c_str());
        lost_primary_replicasets_.insert(replicaset->name);
        break;
      case metadata_cache::InstanceStatus::Unreachable:
        log_warning("Primary instance '%s:%i' [%s] of replicaset '%s' is unreachable. Increasing metadata cache refresh frequency.",
                    instance->host.c_str(), instance->port, instance_id.c_str(),
                    replicaset->name.c_str());
        lost_primary_replicasets_.insert(replicaset->name);
        break;
      case metadata_cache::InstanceStatus::Unusable:
        break;
    }
  }
}

bool MetadataCache::wait_primary_failover(const std::string &replicaset_name,
                                          int timeout) {
  log_debug("Waiting for failover to happen in '%s' for %is",
            replicaset_name.c_str(), timeout);
  time_t stime = std::time(NULL);
  while (std::time(NULL) - stime <= timeout) {
    {
      std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
      if (lost_primary_replicasets_.find(replicaset_name) == lost_primary_replicasets_.end()) {
        return true;
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return false;
}
