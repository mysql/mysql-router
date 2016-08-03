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

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/metadata_cache.h"

#include <chrono>
#include <iostream>
#include <thread>

const std::string kTestReplicaset_1 = "replicaset-1";  // replicaset-1
const std::string kTestReplicaset_2 = "replicaset-2";  // replicaset-1
const std::string kTestReplicaset_3 = "replicaset-3";  // replicaset-1
const std::string kDefaultMetadataHost = "localhost";  // 127.0.0.1
const std::string kDefaultMetadataUser = "root";  // admin
const std::string kDefaultMetadataPassword = "";  //
const int kDefaultMetadataPort = 13001; // 32275
const int kDefaultTTL = 10;
const std::string kDefaultMetadataReplicaset = "replicaset-1";
const int kTotalRuns = 1;

const mysqlrouter::TCPAddress bootstrap_server(kDefaultMetadataHost,
                                               kDefaultMetadataPort);
const std::vector<mysqlrouter::TCPAddress> bootstrap_server_vector =
{bootstrap_server};

using std::cout;
using std::endl;
using std::thread;
using metadata_cache::ManagedInstance;

void print_instance_dump(const std::vector<ManagedInstance> &instance_vector) {
  for (auto&& s: instance_vector) {

    cout << endl;
    cout << "Host: " << s.host << endl;
    cout << "Port: " << s.port << endl;
    cout << "Mode: " << s.mode << endl;
    cout << "Role: " << s.role << endl;
    cout << "Weight: " << s.weight << endl;
  }
}

void print_instance_condensed(const std::vector<ManagedInstance> &instance_vector) {
  if (!instance_vector.size()) {
    cout << "Nothing available" << endl;
    return;
  }
  for (auto&& s: instance_vector) {

    cout << "ManagedInstance: " << s.host << ":" << s.port << " (" << s.mode << ", " << s.role << ")" << endl;
  }
}

int main() {
  std::vector<ManagedInstance> instance_vector_1;
  std::vector<ManagedInstance> instance_vector_2;
   try {
    metadata_cache::cache_init(bootstrap_server_vector,
                               kDefaultMetadataUser, kDefaultMetadataPassword,
                               kDefaultTTL, kDefaultMetadataReplicaset);
    std::this_thread::sleep_for(std::chrono::seconds(5));
  } catch (const std::runtime_error &exc) {
    cout << exc.what() << endl;
    return 0;
  }

  int runs = kTotalRuns;
  while (runs-- > 0) {
    std::cout << "Runs to go " << runs << std::endl;

    std::cout << std::endl << "Test Replicaset 1" << std::endl;

    try {
      instance_vector_1 = metadata_cache::lookup_replicaset(kTestReplicaset_1).
        instance_vector;
      print_instance_condensed(instance_vector_1);
    } catch (const std::runtime_error &exc) {
      cout << exc.what() << endl;
      return 0;
    }

    std::cout << std::endl << "Test Replicaset 2" << std::endl;

    try {
      instance_vector_1 = metadata_cache::lookup_replicaset(kTestReplicaset_2).
        instance_vector;
      print_instance_condensed(instance_vector_1);
    } catch (const std::runtime_error &exc) {
      cout << exc.what() << endl;
      return 0;
    }

    std::cout << std::endl << "Test Replicaset 3" << std::endl;

    try {
      instance_vector_1 = metadata_cache::lookup_replicaset(kTestReplicaset_3).
        instance_vector;
      print_instance_condensed(instance_vector_1);
    } catch (const std::runtime_error &exc) {
      cout << exc.what() << endl;
      return 0;
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));
  }

  return 0;
}
