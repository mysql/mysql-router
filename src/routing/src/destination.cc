/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "common.h"
#include "destination.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/utils.h"
#include "utils.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#ifndef _WIN32
#  include <netdb.h>
#  include <netinet/tcp.h>
#  include <sys/socket.h>
#else
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif


using mysqlrouter::to_string;
using mysqlrouter::TCPAddress;
using std::out_of_range;
IMPORT_LOG_FUNCTIONS()

void RouteDestination::add(const TCPAddress dest) {
  auto dest_end = destinations_.end();

  auto compare = [&dest](TCPAddress &other) { return dest == other; };

  if (std::find_if(destinations_.begin(), dest_end, compare) == dest_end) {
    std::lock_guard<std::mutex> lock(mutex_update_);
    destinations_.push_back(dest);
  }
}

void RouteDestination::add(const std::string &address, uint16_t port) {
  add(TCPAddress(address, port));
}

void RouteDestination::remove(const std::string &address, uint16_t port) {
  TCPAddress to_remove(address, port);
  std::lock_guard<std::mutex> lock(mutex_update_);

  auto func_same = [&to_remove](TCPAddress a) {
    return (a.addr == to_remove.addr && a.port == to_remove.port);
  };
  destinations_.erase(std::remove_if(destinations_.begin(), destinations_.end(), func_same), destinations_.end());

}

TCPAddress RouteDestination::get(const std::string &address, uint16_t port) {
  TCPAddress needle(address, port);
  for (auto &it: destinations_) {
    if (it == needle) {
      return it;
    }
  }
  throw out_of_range("Destination " + needle.str() + " not found");
}

size_t RouteDestination::size() noexcept {
  return destinations_.size();
}

void RouteDestination::clear() {
  if (destinations_.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_update_);
  destinations_.clear();
}

size_t RouteDestination::get_next_server() {
  std::lock_guard<std::mutex> lock(mutex_update_);

  if (destinations_.empty()) {
    throw std::runtime_error("Destination servers list is empty");
  }

  auto result = current_pos_.load();
  current_pos_++;
  if (current_pos_ >= destinations_.size()) current_pos_ = 0;

  return result;
}

int RouteDestination::get_mysql_socket(const TCPAddress &addr, std::chrono::milliseconds connect_timeout, const bool log_errors) {
  return socket_operations_->get_mysql_socket(addr, connect_timeout, log_errors);
}
