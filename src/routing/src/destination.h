/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

  /** @brief Destructor */
  ~RouteDestination();

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
   * -1 when an error occurred, which means that no destination was
   * available.
   *
   * @param connect_timeout About of seconds before timing out
   * @param error Pointer to int for storing errno
   * @return a socket descriptor
   */
  virtual int get_server_socket(int connect_timeout, int *error) noexcept;

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

  /** @brief Returns number of quarantined servers
   *
   * @return size_t
   */
  const size_t size_quarantine();

  /** @brief Start the destination threads
   *
   */
  virtual void start() {
    if (!quarantine_thread_.joinable()) {
      quarantine_thread_ = std::thread(&RouteDestination::quarantine_manager_thread, this);
    } else {
      log_debug("Tried to restart quarantine thread");
    }
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

  /** @brief Adds server to quarantine
   *
   * Adds the given server address to the quarantine list. The index argument
   * is the index of the server in the destination list.
   *
   * @param index Index of the destination
   */
  virtual void add_to_quarantine(size_t index) noexcept;

  /** @brief Worker checking and removing servers from quarantine
   *
   * This method is meant to run in a thread and calling the
   * `cleanup_quarantine()` method.
   *
   * The caller is responsible for locking and unlocking the
   * mutex `mutex_quarantine_`.
   *
   */
  virtual void quarantine_manager_thread() noexcept;

  /** @brief Checks and removes servers from quarantine
   *
   * This method removes servers from quarantine while trying to establish
   * a connection. It is used in a seperate thread and will update the
   * quarantine list, and will keep trying until the list is empty.
   * A conditional variable is used to notify the thread servers were
   * quarantined.
   *
   */
  virtual void cleanup_quarantine() noexcept;

  /** @brief Returns socket descriptor of connected MySQL server
   *
   * Returns a socket descriptor for the connection to the MySQL Server or
   * -1 when an error occurred.
   *
   * This method uses the free function routing::get_mysql_socket.
   *
   * @param addr information of the server we connect with
   * @param connect_timeout number of seconds waiting for connection
   * @param log whether to log errors or not
   * @return a socket descriptor
   */
  virtual int get_mysql_socket(const TCPAddress &addr, int connect_timeout, bool log_errors = true);

  /** @brief List of destinations */
  AddrVector destinations_;

  /** @brief Destination which will be used next */
  std::atomic<size_t> current_pos_;

  /** @brief Whether we are stopping */
  std::atomic_bool stopping_;

  /** @brief Mutex for updating destinations and iterator */
  std::mutex mutex_update_;

  /** @brief List of destinations which are quarantined */
  std::vector<size_t> quarantined_;

  /** @brief Conditional variable blocking quarantine manager thread */
  std::condition_variable condvar_quarantine_;

  /** @brief Mutex for quarantine manager thread */
  std::mutex mutex_quarantine_manager_;

  /** @brief Mutex for updating quarantine */
  std::mutex mutex_quarantine_;

  /** @brief Quarantine manager thread */
  std::thread quarantine_thread_;
};


#endif // ROUTING_DESTINATION_INCLUDED
