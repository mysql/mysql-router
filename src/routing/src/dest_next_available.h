/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTING_DEST_NEXT_AVAILABLE
#define ROUTING_DEST_NEXT_AVAILABLE

#include "destination.h"
#include "mysqlrouter/routing.h"

#include "mysql/harness/logging/logging.h"

class DestNextAvailable final : public RouteDestination {
 public:
  using RouteDestination::RouteDestination;

  int get_server_socket(std::chrono::milliseconds connect_timeout, int *error) noexcept override;
};


#endif // ROUTING_DEST_NEXT_AVAILABLE
