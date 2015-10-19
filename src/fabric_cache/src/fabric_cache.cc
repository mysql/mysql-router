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

#include "fabric_cache.h"

#include <list>
#include <memory>

const map<string, int> FabricCache::shard_type_map_{
    {"RANGE",          RANGE},
    {"RANGE_INTEGER",  RANGE_INTEGER},
    {"RANGE_DATETIME", RANGE_DATETIME},
    {"RANGE_STRING",   RANGE_STRING},
    {"HASH",           HASH}
};

std::map<ManagedServer::Mode, string> ManagedServer::ModeNames{
    {Mode::kOffline,   "offline"},
    {Mode::kReadOnly,  "read-only"},
    {Mode::kWriteOnly, "write-only"},
    {Mode::kReadWrite, "read-write"},
};

std::map<ManagedServer::Status, string> ManagedServer::StatusNames{
    {Status::kFaulty,      "faulty"},
    {Status::kSpare,       "spare"},
    {Status::kSecondary,   "secondary"},
    {Status::kPrimary,     "primary"},
    {Status::kConfiguring, "configuring"},
};

/**
 * Initialize a connection to the MySQL Fabric server.
 *
 * @param host The host on which the fabric server is running.
 * @param port The port number on which the fabric server is listening.
 * @param user The user name used to authenticate to the fabric server.
 * @param password The password used to authenticate to the fabric server.
 * @param fabric_connection_timeout The time after which a connection to the
 *                                  fabric server should timeout.
 * @param connection_attempts The number of times a connection to fabric must be
 *                            attempted, when a connection attempt fails.
 */
FabricCache::FabricCache(string host, int port, string user, string password,
                         int connection_timeout, int connection_attempts) {
  fabric_meta_data_ = get_instance(host, port, user, password,
                                   connection_timeout, connection_attempts);
  ttl_ = kDefaultTimeToLive;
  terminate_ = false;

  refresh();
}

FabricCache::~FabricCache() {
  terminate_ = true;
  if (refresh_thread_.joinable()) {
    refresh_thread_.join();
  }
}

void FabricCache::start() {
  // Start the Fabric Cache refresh thread
  auto refresh_loop = [this] {
    while (!terminate_) {
      if (fabric_meta_data_->connect()) {
        refresh();
      } else {
        fabric_meta_data_->disconnect();
      }
      std::this_thread::sleep_for(
          std::chrono::seconds(ttl_ == 0 ? kDefaultTimeToLive : ttl_));
    }
  };
  thread(refresh_loop).join();
}

list<ManagedServer> FabricCache::group_lookup(const string &group_id) {
  std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
  auto group = group_data_.find(group_id);
  if (group == group_data_.end()) {
    log_warning("Fabric Group '%s' not available", group_id.c_str());
    return {};
  }
  list<ManagedServer> servers = group_data_[group_id];
  return servers;
}

list<ManagedServer> FabricCache::shard_lookup(const string &table_name, const string &shard_key) {
  list<ManagedServer> servers;
  cache_refreshing_mutex_.lock();
  if (shard_data_.count(table_name)) {
    std::unique_ptr<ManagedShard> ret(nullptr);
    list<ManagedShard> shards = shard_data_[table_name];
    std::unique_ptr<ValueComparator> vc(fetch_value_comparator(shards.front().type_name));
    for (auto &&s : shards) {
      int cmp = vc->compare(shard_key, s.lb);
      if (cmp == 0 || cmp == 1) {
        if (ret == nullptr) {
          ret.reset(new ManagedShard);
          copy(s, *ret);
        }
        else if (vc->compare(s.lb, ret->lb) == 1) {
          //The  shard with the smallest lower bound greater than the
          //shard key is the shard in which the shard key should be
          //placed.
          ret.reset(new ManagedShard);
          copy(s, *ret);
        }
        else {
          continue;
        }
      }
    }
    if (ret) {
      servers = group_data_[ret->group_id];
    } else {
      servers = {};
    }
  }
  cache_refreshing_mutex_.unlock();
  return servers;
}

void FabricCache::refresh() {
  try {
    fetch_data();
    cache_refreshing_mutex_.lock();
    group_data_ = group_data_temp_;
    shard_data_ = shard_data_temp_;
    cache_refreshing_mutex_.unlock();
  } catch (const fabric_cache::base_error &exc) {
    log_debug("Failed fetching data: %s", exc.what());
  }
}

void FabricCache::fetch_data() {
    group_data_temp_ = fabric_meta_data_->fetch_servers();
    shard_data_temp_ = fabric_meta_data_->fetch_shards();
    ttl_ = fabric_meta_data_->fetch_ttl();
}

ValueComparator *FabricCache::fetch_value_comparator(string shard_type) {
  std::transform(shard_type.begin(), shard_type.end(),
                 shard_type.begin(), ::toupper);
  switch (shard_type_map_.at(shard_type)) {
    case RANGE:
    case RANGE_INTEGER:
      return new IntegerValueComparator();
      break;
    case RANGE_DATETIME:
      return new DateTimeValueComparator();
      break;
    case RANGE_STRING:
      return new StringValueComparator();
      break;
    case HASH:
      return new MD5HashValueComparator();
      break;
    default:
      return nullptr;
      break;
  }
}

void FabricCache::copy(const ManagedShard &source_shard, ManagedShard &destn_shard) {
  destn_shard.schema_name = source_shard.schema_name;
  destn_shard.table_name = source_shard.table_name;
  destn_shard.column_name = source_shard.column_name;
  destn_shard.lb = source_shard.lb;
  destn_shard.shard_id = source_shard.shard_id;
  destn_shard.type_name = source_shard.type_name;
  destn_shard.group_id = source_shard.group_id;
  destn_shard.global_group = source_shard.global_group;
}
