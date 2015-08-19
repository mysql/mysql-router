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

#ifndef ROUTING_DESTINATION_INCLUDED
#define ROUTING_DESTINATION_INCLUDED

#include "config.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "mysqlrouter/datatypes.h"

using mysqlrouter::TCPAddress;
using std::string;

/** @class RouteDestination
 * @brief Manage destinations for a Connection Routing
 *
 * This class manages destinations which are used in Connection Routing.
 * A destination is usually a MySQL Server and is stored using the IP
 * or hostname together with the TCP port (defaulting to 3306).
 *
 * RouteDestination is meant to be a base class and used to inherite and
 * create class which change the behavior. For example, the `get_next()`
 * method is usually changed to get the next server in the list.
 */
class RouteDestination {
public:

  using addr_vector = std::vector<TCPAddress>;

  /** @brief Default constructor */
  RouteDestination() : destination_iter_(destinations_.begin()) {};

  /** @brief Adds a destination
   *
   * Adds a destination using the given address and port number.
   *
   * @param address IP or name
   * @param port Port number
   */
  virtual void add(const TCPAddress dest);

  /** @overload */
  virtual void add(const string &address, uint16_t port);



  /** @brief Removes a destination
   *
   * Removes a destination using the given address and port number.
   *
   * @param address IP or name
   * @param port Port number
   */
  virtual void remove(const string &address, uint16_t port);

  /** @brief Gets destination based on address and port
   *
   * Gets destination base on given address and port and returns a pair
   * with the information.
   *
   * Raises std::out_of_range when the combination of address and port
   * is not in the list of destinations.
   *
   * This function can be used to check whether given destination is in
   * the list.
   *
   * @param address IP or name
   * @param port Port number
   * @return an instance of mysqlrouter::TCPAddress
   */
  virtual TCPAddress get(const string &address, uint16_t port);

  /** @brief Removes all destinations
   *
   * Removes all destinations from the list.
   */
  virtual void clear();

  /** @brief Gets next connection to destination
   *
   * Get a connection to the next available destination.
   *
   * @return Instance of mysqlrouter::TCPAddress
   */
  virtual const TCPAddress get_next() noexcept;

  /** @brief Gets the number of destinations
   *
   * Gets the number of destinations currently in the list.
   *
   * @return Number of destinations as size_t
   */
  size_t size() noexcept;

  /** @brief Returns whether there are destinations
   *
   * @return whether the destination is empty
   */
  bool empty() noexcept {
    return destinations_.empty();
  }

  /** @brief Rewinds iterator to the beginning
   *
   * By default this method does not do anything.
   */
  virtual void rewind() noexcept {};

  addr_vector::iterator begin() {
    return destinations_.begin();
  }

  addr_vector::const_iterator begin() const {
    return destinations_.begin();
  }

  addr_vector::iterator end() {
    return destinations_.end();
  }

  addr_vector::const_iterator end() const {
    return destinations_.end();
  }

protected:
  /** @brief Executes actions after adding destinatino
   *
 * By default, the iterator over destinations is
 * reset to the first element.
   */
  virtual void post_add() {
    destination_iter_ = destinations_.begin();
  }

  /** @brief Executes actions after removing destinatino
 *
 * By default, the iterator over destinations is
 * reset to the first element.
 */
  virtual void post_remove() {
    destination_iter_ = destinations_.begin();
  };

  /** @brief List of destinations */
  addr_vector destinations_;
  /** @brief Next destination that can be used */
  addr_vector::iterator destination_iter_;
  /** @brief Mutex for updating destinations and iterator */
  std::mutex mutex_update_;
};


#endif // ROUTING_DESTINATION_INCLUDED
