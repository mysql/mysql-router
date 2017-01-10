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

#include <memory>

#include "mock_metadata.h"

std::shared_ptr<MetaData> meta_data;

/**
 * Create an instance of the mock metadata.
 *
 * @param user The user name used to authenticate to the metadata server.
 * @param password The password used to authenticate to the metadata server.
 * @param connection_timeout The time after which a connection to the
 * @param connection_attempts The number of times a connection to metadata must be
 *                            attempted, when a connection attempt fails.
 * @param ttl The TTL of the cached data.
 * @param ssl_mode (unused)
 */
std::shared_ptr<MetaData> get_instance(
  const std::string &user,
  const std::string &password,
  int connection_timeout,
  int connection_attempts,
  unsigned int ttl,
  const std::string & /*ssl_mode*/) {
  meta_data.reset(new MockNG(user, password, connection_timeout,
                             connection_attempts, ttl));
  return meta_data;
}
