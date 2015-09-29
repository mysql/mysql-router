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

#ifndef FABRIC_CACHE_FABRIC_METADATA_INCLUDED
#define FABRIC_CACHE_FABRIC_METADATA_INCLUDED

#include "mysqlrouter/fabric_cache.h"

#include <list>
#include <map>
#include <string>

using std::list;
using std::map;
using std::string;
using fabric_cache::ManagedServer;
using fabric_cache::ManagedShard;

/**
 * The fabric metadata class is used to create a pluggable transport layer
 * from which the metadata is fetched for the fabric cache.
 */
class FabricMetaData {
public:
  virtual int fetch_ttl() = 0;
  virtual map<string, list<ManagedServer>> fetch_servers() = 0;
  virtual map<string, list<ManagedShard>> fetch_shards() = 0;
  virtual bool connect() = 0;
  virtual void disconnect() = 0;
};

#endif // FABRIC_CACHE_FABRIC_METADATA_INCLUDED
