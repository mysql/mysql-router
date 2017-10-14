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

#include "metadata_factory.h"
#include "cluster_metadata.h"

std::shared_ptr<MetaData> meta_data{nullptr};

/**
 * Return an instance of NG metadata.
 *
 * @param user The user name used to authenticate to the metadata server.
 * @param password The password used to authenticate to the metadata server.
 * @param connect_timeout The time after which trying to connect to the
 *                        metadata server should timeout.
 * @param read_timeout The time after which read from metadata server should
 *                     timeout.
 * @param connection_attempts The number of times a connection to the metadata
 *                            server must be attempted, when a connection
 *                            attempt fails.
 * @param ttl The TTL of the cached data.
 * @param ssl_options SSL related options to be used for connection
 */
std::shared_ptr<MetaData> get_instance(
  const std::string &user,
  const std::string &password,
  int connect_timeout,
  int read_timeout,
  int connection_attempts,
  unsigned int ttl,
  const mysqlrouter::SSLOptions &ssl_options
  ) {
  meta_data.reset(new ClusterMetadata(user, password, connect_timeout,
                                      read_timeout, connection_attempts, ttl,
                                      ssl_options));
  return meta_data;
}
