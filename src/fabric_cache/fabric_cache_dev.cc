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

#include "mysqlrouter/fabric_cache.h"

#include <chrono>
#include <iostream>
#include <thread>

const string kDefaultTestGroup = "group-1";  // group-1
const string kDefaultTestShardTable = "db1.t1";  // db1.t1
const string kTestShardKey = "100";  // 100
const string kDefaultFabricHost = "127.0.0.1";  // 127.0.0.1
const string kDefaultFabricUser = "admin";  // admin
const string kDefaultFabricPassword = "";  //
const int kDefaultFabricPort = 32275; // 32275
const int kTotalRuns = 1;

using std::cout;
using std::endl;
using std::thread;
using fabric_cache::ManagedServer;

void print_server_dump(list<ManagedServer> server_list) {
  for (auto&& s: server_list) {

    auto status = ManagedServer::StatusNames[static_cast<ManagedServer::Status>(s.status)];
    auto mode = ManagedServer::ModeNames[static_cast<ManagedServer::Mode>(s.mode)];

    cout << endl;
    cout << "FabricManagedServer ID: " << s.server_uuid << endl;
    cout << "Host: " << s.host << endl;
    cout << "Port: " << s.port << endl;
    cout << "Mode: " << s.mode << " (" << mode << ")" << endl;
    cout << "Status: " << s.status << " (" << status << ")" << endl;
    cout << "Weight: " << s.weight << endl;
  }
}

void print_server_condensed(list<ManagedServer> server_list) {
  if (!server_list.size()) {
    cout << "Nothing available" << endl;
    return;
  }
  for (auto&& s: server_list) {

    auto status = ManagedServer::StatusNames[static_cast<ManagedServer::Status>(s.status)];
    auto mode = ManagedServer::ModeNames[static_cast<ManagedServer::Mode>(s.mode)];

    cout << "FabricManagedServer: " << s.host << ":" << s.port << " (" << mode << ", " << status << ")" << endl;
  }
}

int main() {
  list<ManagedServer> server_list_1;
  list<ManagedServer> server_list_2;
  string cache_name = "maintest";
  try {
    thread connect_thread(
      fabric_cache::cache_init, cache_name, kDefaultFabricHost,
      kDefaultFabricPort, kDefaultFabricUser, kDefaultFabricPassword);
    connect_thread.detach();
    std::this_thread::sleep_for(std::chrono::seconds(5));
  } catch (const fabric_cache::base_error &exc) {
    cout << exc.what() << endl;
    return 0;
  }

  int runs = kTotalRuns;
  while (runs-- > 0) {
    std::cout << "Runs to go " << runs << std::endl;
    try {
      server_list_1 = fabric_cache::lookup_group(cache_name, kDefaultTestGroup).
        server_list;
      print_server_condensed(server_list_1);
      server_list_2 = fabric_cache::lookup_shard(cache_name,
                                                 kDefaultTestShardTable,
                                                 kTestShardKey).server_list;
      print_server_condensed(server_list_1);
    } catch (const fabric_cache::base_error &exc) {
      cout << exc.what() << endl;
      return 0;
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }

  return 0;
}
