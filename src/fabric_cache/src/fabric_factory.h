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

#ifndef FABRIC_CACHE_FABRIC_FACTORY_INCLUDED
#define FABRIC_CACHE_FABRIC_FACTORY_INCLUDED

#include "fabric_metadata.h"

#include <memory>

//This provides a factory method that returns a pluggable instance
//to the underlying transport layer implementation. The transport
//layer provides the means from which the fabric cache metadata is
//fetched.

std::shared_ptr<FabricMetaData> get_instance(const string &host, int port,
                                             const string &user,
                                             const string &password,
                                             int connection_timeout,
                                             int connection_attempts);

#endif // FABRIC_CACHE_FABRIC_FACTORY_INCLUDED
