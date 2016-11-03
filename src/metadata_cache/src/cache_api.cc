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

#include "mysqlrouter/metadata_cache.h"
#include "metadata_cache.h"

#include <map>
#include <memory>

static std::unique_ptr<MetadataCache> g_metadata_cache(nullptr);

namespace metadata_cache {

const uint16_t kDefaultMetadataPort = 32275;
const unsigned int kDefaultMetadataTTL = 5 * 60;
const std::string kDefaultMetadataAddress{"127.0.0.1:" + mysqlrouter::to_string(
    kDefaultMetadataPort)};
const std::string kDefaultMetadataUser = "";
const std::string kDefaultMetadataPassword = "";
const std::string kDefaultMetadataCluster = ""; // blank cluster name means pick the 1st (and only) cluster

/**
 * Initialize the metadata cache.
 *
 * @param bootstrap_servers The initial set of servers that contain the server
 *                          topology metadata.
 * @param user The user name used to connect to the metadata servers.
 * @param password The password used to connect to the metadata servers.
 * @param ttl The ttl for the contents of the cache
 * @param cluster_name The name of the cluster from the metadata schema
 */
void cache_init(const std::vector<mysqlrouter::TCPAddress> &bootstrap_servers,
                  const std::string &user,
                  const std::string &password,
                  unsigned int ttl,
                  const std::string &cluster_name) {
  g_metadata_cache.reset(new MetadataCache(bootstrap_servers, user, password, 1,
                                           1, ttl, cluster_name));
  g_metadata_cache->start();
}

/**
 * Lookup the servers that belong to the given replicaset.
 *
 * @param replicaset_name The name of the replicaset whose servers need
 *                      to be looked up.
 *
 * @return An object that encapsulates a list of managed MySQL servers.
 *
 */
LookupResult lookup_replicaset(const std::string &replicaset_name) {

  if (g_metadata_cache == nullptr) {
    throw std::runtime_error("Metadata Cache not initialized");
  }

  return LookupResult(g_metadata_cache->replicaset_lookup(replicaset_name));
}


void mark_instance_reachability(const std::string &instance_id,
                                InstanceStatus status) {
  if (g_metadata_cache == nullptr) {
    throw std::runtime_error("Metadata Cache not initialized");
  }

  g_metadata_cache->mark_instance_reachability(instance_id, status);
}
} // namespace metadata_cache
