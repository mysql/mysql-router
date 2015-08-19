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

#include "destination.h"
#include "utils.h"

#include <algorithm>
#include <iostream>
#include <netdb.h>
#include <netinet/tcp.h>

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"
#include "logger.h"

using mysqlrouter::to_string;
using mysqlrouter::TCPAddress;
using std::out_of_range;
using std::runtime_error;

void RouteDestination::add(const TCPAddress dest) {
  auto dest_end = destinations_.end();

  auto compare = [&dest](TCPAddress &other) { return dest == other; };

  if (std::find_if(destinations_.begin(), dest_end, compare) == dest_end) {
    std::lock_guard<std::mutex> lock(mutex_update_);
    destinations_.push_back(dest);
    post_add();
  }
}

void RouteDestination::add(const string &address, uint16_t port) {
  add(TCPAddress(address, port));
}

void RouteDestination::remove(const string &address, uint16_t port) {
  TCPAddress to_remove(address, port);
  std::lock_guard<std::mutex> lock(mutex_update_);

  auto func_same = [&to_remove](TCPAddress a) {
    return (a.addr == to_remove.addr && a.port == to_remove.port);
  };
  destinations_.erase(std::remove_if(destinations_.begin(), destinations_.end(), func_same), destinations_.end());

  post_remove();
}

TCPAddress RouteDestination::get(const string &address, uint16_t port) {
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
  post_remove();
}

const TCPAddress RouteDestination::get_next() noexcept {
  if (destinations_.empty()) {
    return TCPAddress();
  }

  // With only 1 destination, no need to lock and update the iterator
  if (destinations_.size() == 1) {
    return destinations_.at(0);
  }

  std::lock_guard<std::mutex> lock(mutex_update_);
  auto result = *destination_iter_++;  // get current and advance one
  if (destination_iter_ == destinations_.end()) {
    destination_iter_ = destinations_.begin();
  }
  return result;
}
