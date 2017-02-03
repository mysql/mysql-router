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

#ifndef METADATA_CACHE_METADATA_FACTORY_INCLUDED
#define METADATA_CACHE_METADATA_FACTORY_INCLUDED

#include <memory>

#include "metadata.h"
#include "mysqlrouter/datatypes.h"

//This provides a factory method that returns a pluggable instance
//to the underlying transport layer implementation. The transport
//layer provides the means from which the metadata is
//fetched.

std::shared_ptr<MetaData> get_instance(
  const std::string &user, const std::string &password, int connection_timeout,
  int connection_attempts, unsigned int ttl,
  const mysqlrouter::SSLOptions &ssl_options);

#endif // METADATA_CACHE_METADATA_FACTORY_INCLUDED
