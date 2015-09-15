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

#include "fabric_factory.h"
#include "fabric.h"

#include <memory>
#include <mutex>

std::once_flag fabric_metadata_flag;

std::shared_ptr<FabricMetaData> fabric_meta_data(nullptr);

/**
 * Create an instance of fabric metadata.
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
void create_instance(const string &host, int port, const string &user,
                     const string &password, int connection_timeout,
                     int connection_attempts) {
  fabric_meta_data.reset(new Fabric(host, port, user, password,
                                    connection_timeout, connection_attempts));
}

/**
 * Get a fabric metadata fetch instance.
 *
 * @param host The host on which the fabric server is running.
 * @param port The port number on which the fabric server is listening.
 * @param user The user name used to authenticate to the fabric server.
 * @param password The password used to authenticate to the fabric server.
 * @param fabric_connection_timeout The time after which a connection to the
 *                                  fabric server should timeout.
 * @param connection_attempts The number of times a connection to fabric must be
 *                            attempted, when a connection attempt fails.
 *
 * @return An instance of the fabric meta data fetcher instance.
 */
std::shared_ptr<FabricMetaData> get_instance(const string &host, int port,
                                             const string &user,
                                             const string &password,
                                             int connection_timeout,
                                             int connection_attempts) {
  if (fabric_meta_data) {
    return fabric_meta_data;
  }
  std::call_once(fabric_metadata_flag, create_instance, host, port, user,
                 password, connection_timeout, connection_attempts);
  return fabric_meta_data;
}
