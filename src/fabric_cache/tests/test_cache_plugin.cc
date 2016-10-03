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

/**
 * Tests the fabric cache plugin implementation.
 */

#include "mock_fabric.h"
#include "mysqlrouter/fabric_cache.h"

#include <chrono>
#include <list>
#include <thread>

#include "gmock/gmock.h"

using std::list;
using std::string;
using std::thread;
using fabric_cache::ManagedServer;

/**
 * Constants that are used throughout the test cases.
 */

const string kDefaultTestGroup_1 = "group-1";  // group-1
const string kDefaultTestGroup_2 = "group-2";  // group-2

const string kDefaultTestShardTable = "db1.t1";  // db1.t1
const string kTestShardKey_1 = "100";  // 100
const string kTestShardKey_2 = "1000";  // 1000
const string kTestShardKey_3 = "10000";  // 10000
const string kDefaultFabricHost = "127.0.0.1";  // 127.0.0.1
const string kDefaultFabricUser = "admin";  // admin
const string kDefaultFabricPassword = "";  //
const int kDefaultFabricPort = 32275; // 32275

using std::thread;
using fabric_cache::ManagedServer;
using fabric_cache::MockFabric;

class FabricCachePluginTest : public ::testing::Test {
public:
  string cache_name = "maintest";
  MockFabric mf;

  FabricCachePluginTest() : mf(kDefaultFabricHost, kDefaultFabricPort,
                               kDefaultFabricUser, kDefaultFabricPassword,
                               1, 1) {}

  virtual void SetUp() {
    list<ManagedServer> server_list_1;
    thread connect_thread(
      fabric_cache::cache_init, cache_name, kDefaultFabricHost,
      kDefaultFabricPort, kDefaultFabricUser, kDefaultFabricPassword);
    connect_thread.detach();

    int count = 1;
    /**
     * Wait until the plugin is completely initialized. Since
     * the plugin initialization is started on a separate thread,
     * we are required to wait until the cache is populated.
     */
    while (server_list_1.size() != 2) {
      try {
        server_list_1 = fabric_cache::lookup_group(cache_name,
                                                 kDefaultTestGroup_1).
          server_list;
      } catch (const fabric_cache::base_error &exc) {
        /**
         * If the lookup fails after 5 attempts it points to an error
         * in the cache initialization. This is an exception situation.
         */
        if (count++ >= 5)
          throw exc;
      }
      /**
       * Sleep before retrying the lookup.
       */
#ifndef _WIN32
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
#else
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
#endif
    }
  }
};

/**
 * Test that looking up an invalid group returns a empty list.
 */
TEST_F(FabricCachePluginTest, InvalidGroupTest) {
  EXPECT_TRUE(fabric_cache::lookup_group(cache_name, "InvalidGroup").
              server_list.empty());
}

/**
 * Test that the list of servers that are part of a group is accurate.
 */
TEST_F(FabricCachePluginTest, ValidGroupTest) {
  list<ManagedServer> server_list_1 = fabric_cache::lookup_group(cache_name,
                                             kDefaultTestGroup_1).server_list;
  ManagedServer ms1_fetched = server_list_1.front();
  EXPECT_EQ(ms1_fetched, mf.ms1);
  server_list_1.pop_front();
  ManagedServer ms2_fetched = server_list_1.front();
  EXPECT_EQ(ms2_fetched, mf.ms2);
}

/**
 * Test the list of servers that are part of a shard in fabric.
 */
TEST_F(FabricCachePluginTest, ValidShardTest_1) {
  list<ManagedServer> server_list_2 = fabric_cache::lookup_shard(cache_name,
                                             kDefaultTestShardTable,
                                             kTestShardKey_1).server_list;
  ManagedServer ms3_fetched = server_list_2.front();
  EXPECT_EQ(ms3_fetched, mf.ms3);
  server_list_2.pop_front();
  ManagedServer ms4_fetched = server_list_2.front();
  EXPECT_EQ(ms4_fetched, mf.ms4);
}

/**
 * Test the list of servers that are part of a shard in fabric.
 */
TEST_F(FabricCachePluginTest, ValidShardTest_2) {
  list<ManagedServer> server_list_2 = fabric_cache::lookup_shard(cache_name,
                                             kDefaultTestShardTable,
                                             kTestShardKey_2).server_list;
  ManagedServer ms5_fetched = server_list_2.front();
  EXPECT_EQ(ms5_fetched, mf.ms5);
  server_list_2.pop_front();
  ManagedServer ms6_fetched = server_list_2.front();
  EXPECT_EQ(ms6_fetched, mf.ms6);
}

/**
 * Test the list of servers that are part of a shard in fabric.
 */
TEST_F(FabricCachePluginTest, ValidShardTest_3) {
  list<ManagedServer> server_list_2 = fabric_cache::lookup_shard(cache_name,
                                             kDefaultTestShardTable,
                                             kTestShardKey_3).server_list;
  ManagedServer ms5_fetched = server_list_2.front();
  EXPECT_EQ(ms5_fetched, mf.ms5);
  server_list_2.pop_front();
  ManagedServer ms6_fetched = server_list_2.front();
  EXPECT_EQ(ms6_fetched, mf.ms6);
}

/**
 * Test that the list of servers in a invalid shard is empty.
 */
TEST_F(FabricCachePluginTest, InvalidShardTest) {
  list<ManagedServer> server_list_2 = fabric_cache::lookup_shard(cache_name,
                                             "InvalidShardTable",
                                             kTestShardKey_3).server_list;
  EXPECT_TRUE(server_list_2.empty());
}

int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
