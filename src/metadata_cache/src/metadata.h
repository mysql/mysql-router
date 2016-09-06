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

#ifndef METADATA_CACHE_METADATA_INTERFACE_INCLUDED
#define METADATA_CACHE_METADATA_INTERFACE_INCLUDED

#include "mysqlrouter/metadata_cache.h"

#include <vector>
#include <map>
#include <string>

/**
 * The metadata class is used to create a pluggable transport layer
 * from which the metadata is fetched for the metadata cache.
 */
class MetaData {
public:
  typedef std::map<std::string, std::vector<metadata_cache::ManagedInstance>> InstancesByReplicaSet;

  virtual unsigned int fetch_ttl() = 0;
  virtual InstancesByReplicaSet fetch_instances(const std::string &cluster_name) = 0;

  virtual bool connect(const std::vector<metadata_cache::ManagedInstance>
                       & metadata_servers) = 0;
  virtual void disconnect() = 0;
};

#endif // METADATA_CACHE_METADATA_INTERFACE_INCLUDED
