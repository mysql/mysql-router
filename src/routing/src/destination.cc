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
#include "mysqlrouter/routing.h"

#include <algorithm>
#include <iostream>
#include <netdb.h>
#include <netinet/tcp.h>

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"
#include "logger.h"

using mysqlrouter::to_string;
using mysqlrouter::TCPAddress;
using routing::get_mysql_socket;
using std::out_of_range;
using std::runtime_error;

void RouteDestination::add(const TCPAddress dest) {
  auto dest_end = destinations_.end();

  auto compare = [&dest](TCPAddress &other) { return dest == other; };

  if (std::find_if(destinations_.begin(), dest_end, compare) == dest_end) {
    std::lock_guard<std::mutex> lock(mutex_update_);
    destinations_.push_back(dest);
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
}

int RouteDestination::get_server_socket(int connect_timeout) noexcept {

  if (destinations_.empty()) {
    return -1;
  }

  // With only 1 destination, no need to lock and update the iterator
  if (destinations_.size() == 1) {
    return get_mysql_socket(destinations_.at(0), connect_timeout);
  }

  // We start the list at the currently available server
  for (size_t i = current_pos_; i < destinations_.size(); ++i) {
    // Stop when all destinations are quarantined
    if (quarantined_.size() == destinations_.size()) {
      log_debug("No more destinations: all quarantined");
      break;
    }
    if (is_quarantined(i)) {
      if (i + 1 == destinations_.size()) {
        i = 0;
      } else {
        ++i;
      }
    }
    log_debug("Trying server %s", destinations_.at(i).str().c_str());
    auto sock = get_mysql_socket(destinations_.at(i), connect_timeout);
    if (sock != -1) {
      if (i + 1 == destinations_.size()) {
        current_pos_ = 0;
      } else {
        current_pos_ = i + 1;
      }
      return sock;
    }

    log_info("Quarantine destination server %s", destinations_.at(i).str().c_str());
    mutex_update_.lock();
    if (!is_quarantined(i)) {
      quarantined_.push_back(i);
    }
    mutex_update_.unlock();
    condvar_quarantine_.notify_one();

    // If this was the last destination; go back to the first
    if (i + 1 == destinations_.size()) {
      i = -1; // Next iteration will start at 0
    }
  }

  // We are out of destinations. Next time we will try from the beginning of the list.
  condvar_quarantine_.notify_one();
  current_pos_ = 0;
  return -1;
}

void RouteDestination::remove_from_quarantine() noexcept {
  while(!stopping_) {
    std::unique_lock<std::mutex> lock(mutex_quarantine_);
    condvar_quarantine_.wait(lock, [&] { return !quarantined_.empty();});
    for (auto it = quarantined_.begin(); it != quarantined_.end() && !quarantined_.empty(); ++it) {
      auto sock = get_mysql_socket(destinations_.at(*it), 3, false);
      if (sock != -1) {
        log_info("Removing destination server %s from quarantine", destinations_.at(*it).str().c_str());
        mutex_update_.lock();
        quarantined_.erase(it);
        mutex_update_.unlock();
      }
    }
    // Temporize
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }
}