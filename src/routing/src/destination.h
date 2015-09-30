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

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mysqlrouter/datatypes.h"
#include "logger.h"

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

  using AddrVector = std::vector<TCPAddress>;

  /** @brief Default constructor */
  RouteDestination() : current_pos_(0), stopping_(false) { };

  /** @brief destructor */
  ~RouteDestination() {
    stopping_ = true;
    if (quarantine_thread_.joinable()) {
      quarantine_thread_.join();
    }
  }

  RouteDestination(const RouteDestination &other) = delete;
  RouteDestination(RouteDestination &&other) = delete;
  RouteDestination &operator=(const RouteDestination &other) = delete;
  RouteDestination &operator=(RouteDestination &&other) = delete;

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
   * Returns a socket descriptor for the connection to the MySQL Server or
   * -1 when an error occurred.
   *
   * @return a socket descriptor
   */
  virtual int get_server_socket(int connect_timeout) noexcept;

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
  virtual bool empty() const noexcept {
    return destinations_.empty();
  }

  /** @brief Start the destination threads
   *
   */
  virtual void start() {
    quarantine_thread_ = std::thread(&RouteDestination::remove_from_quarantine, this);
  }

  AddrVector::iterator begin() {
    return destinations_.begin();
  }

  AddrVector::const_iterator begin() const {
    return destinations_.begin();
  }

  AddrVector::iterator end() {
    return destinations_.end();
  }

  AddrVector::const_iterator end() const {
    return destinations_.end();
  }

protected:
  /** @brief Returns whether destination is quarantined
   *
   * Uses the given index to check whether the destination is
   * quarantined.
   *
   * @param size_t index of the destination to check
   * @return True if destination is quarantined
   */
  virtual bool is_quarantined(const size_t index) {
    return std::find(quarantined_.begin(), quarantined_.end(), index) != quarantined_.end();
  }

  virtual void remove_from_quarantine() noexcept;


  /** @brief List of destinations */
  AddrVector destinations_;
  /** @brief Destination which will be used next */
  std::atomic<size_t> current_pos_;
  /** @brief Whether we are stopping */
  std::atomic<bool> stopping_;

  /** @brief Mutex for updating destinations and iterator */
  std::mutex mutex_update_;


  /** @brief List of destinations which are quarantined */
  std::vector<size_t> quarantined_;
  std::condition_variable condvar_quarantine_;
  std::mutex mutex_quarantine_;
  std::thread quarantine_thread_;

};


#endif // ROUTING_DESTINATION_INCLUDED
