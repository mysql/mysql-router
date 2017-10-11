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

#include <functional>
#include <iostream>
#include <stdexcept>

#include "mysql/harness/logging/logging.h"
#include "test/helpers.h"
#include "router_test_helpers.h"
#include "destination.h"
#include "dest_metadata_cache.h"

#include "mysqlrouter/datatypes.h"
#include "routing_mocks.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"


using mysqlrouter::TCPAddress;
using ::testing::StrEq;

class RouteDestinationTest : public ::testing::Test {
protected:
  virtual void SetUp() {}

  MockSocketOperations mock_socket_operations_;
};

TEST_F(RouteDestinationTest, Constructor)
{
  RouteDestination d;
  size_t exp = 0;
  ASSERT_EQ(exp, d.size());
}

TEST_F(RouteDestinationTest, Add)
{
  size_t exp;
  RouteDestination d;
  exp = 1;
  d.add("addr1", 1);
  ASSERT_EQ(exp, d.size());
  exp = 2;
  d.add("addr2", 2);
  ASSERT_EQ(exp, d.size());

  // Already added destination
  d.add("addr1", 1);
  exp = 2;
  ASSERT_EQ(exp, d.size());
}

TEST_F(RouteDestinationTest, Remove)
{
  size_t exp;
  RouteDestination d;
  d.add("addr1", 1);
  d.add("addr99", 99);
  d.add("addr2", 2);
  exp = 3;
  ASSERT_EQ(exp, d.size());
  d.remove("addr99", 99);
  exp = 2;
  ASSERT_EQ(exp, d.size());
  d.remove("addr99", 99);
  exp = 2;
  ASSERT_EQ(exp, d.size());
}

TEST_F(RouteDestinationTest, Get)
{
  RouteDestination d;
  ASSERT_THROW(d.get("addr1", 1), std::out_of_range);
  d.add("addr1", 1);
  ASSERT_NO_THROW(d.get("addr1", 1));

  TCPAddress addr = d.get("addr1", 1);
  ASSERT_THAT(addr.addr, StrEq("addr1"));
  EXPECT_EQ(addr.port, 1);

  d.remove("addr1", 1);
  ASSERT_THAT(addr.addr, StrEq("addr1"));
  EXPECT_EQ(addr.port, 1);
}

TEST_F(RouteDestinationTest, Size)
{
  size_t exp;
  RouteDestination d;
  exp = 0;
  ASSERT_EQ(exp, d.size());
  d.add("addr1", 1);
  exp = 1;
  ASSERT_EQ(exp, d.size());
  d.remove("addr1", 1);
  exp = 0;
  ASSERT_EQ(exp, d.size());
}

TEST_F(RouteDestinationTest, RemoveAll)
{
  size_t exp;
  RouteDestination d;

  d.add("addr1", 1);
  d.add("addr2", 2);
  d.add("addr3", 3);
  exp = 3;
  ASSERT_EQ(exp, d.size());

  d.clear();
  exp = 0;
  ASSERT_EQ(exp, d.size());
}

TEST_F(RouteDestinationTest, get_server_socket)
{
  int error;

  // create round-robin (read-only) destination and add a few servers
  RouteDestination dest(Protocol::get_default(), &mock_socket_operations_);
  std::vector<int> dest_servers_addresses { 11, 12, 13 };
  for (const auto& server_address: dest_servers_addresses) {
    dest.add(std::to_string(server_address), 1 /*port - doesn't matter here*/);
  }

  // NOTE: this test exploits the fact that MockSocketOperations::get_mysql_socket() returns the value
  // based on the IP address it is given (it uses the number the address starts with)

  using ThrPtr = std::unique_ptr<std::thread>;
  std::vector<ThrPtr> client_threads;
  std::map<int, size_t> connections; // number of connections per each destination address
  std::mutex connections_mutex;

  // spawn number of threads each trying to get the server socket at the same time
  const size_t kNumClientThreads = dest_servers_addresses.size() * 10;
  for (size_t i = 0; i < kNumClientThreads; ++i) {
    client_threads.emplace_back(
      new std::thread(
        [&]() {
          int addr = dest.get_server_socket(0, &error);
          {
            std::unique_lock<std::mutex> lock(connections_mutex);
            // increment the counter for returned address
            ++connections[addr];
          }
        }
      )
    );
  }

  // wait for each thread to finish
  for (auto& thr: client_threads) {
     thr->join();
  }

  // we didn't simulate any connection errors so there should be no quarantine
  // so the number of connections to the the destination addresses should be evenly distributed
  for (const auto& server_address: dest_servers_addresses) {
    EXPECT_EQ(connections[server_address], kNumClientThreads/dest_servers_addresses.size());
  }
}

TEST_F(RouteDestinationTest, MetadataCacheGroupAllowPrimaryReads)
{
  // yes
  {
    mysqlrouter::URI uri("metadata-cache://test/default?allow_primary_reads=yes&role=SECONDARY");
    ASSERT_NO_THROW(
      DestMetadataCacheGroup dest("metadata_cache_name", "replicaset_name", "read-only",
                                uri.query, Protocol::Type::kClassicProtocol)
    );
  }

  // no
  {
    mysqlrouter::URI uri("metadata-cache://test/default?allow_primary_reads=no&role=SECONDARY");
    ASSERT_NO_THROW(
      DestMetadataCacheGroup dest("metadata_cache_name", "replicaset_name", "read-only",
                                uri.query, Protocol::Type::kClassicProtocol)
    );
  }

  // invalid value
  {
    mysqlrouter::URI uri("metadata-cache://test/default?allow_primary_reads=yes,xxx&role=SECONDARY");
    ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest("metadata_cache_name", "replicaset_name", "read-only",
                                uri.query, Protocol::Type::kClassicProtocol),
      std::runtime_error,
      "Invalid value for allow_primary_reads option: \"yes,xxx\""
    );
  }
}


TEST_F(RouteDestinationTest, MetadataCacheGroupMultipleUris)
{
  {
    mysqlrouter::URI uri("metadata-cache://test/default?role=SECONDARY,metadata-cache://test2/default?role=SECONDARY");
    ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest("metadata_cache_name", "replicaset_name", "read-only",
                                uri.query, Protocol::Type::kClassicProtocol),
      std::runtime_error,
      "Invalid value for role option: \"SECONDARY,metadata-cache://test2/default?role\""
    );
  }
}

TEST_F(RouteDestinationTest, MetadataCacheGroupUnknownParam)
{
  {
    mysqlrouter::URI uri("metadata-cache://test/default?role=SECONDARY&xxx=yyy,metadata-cache://test2/default?role=SECONDARY");
    ASSERT_THROW_LIKE(
      DestMetadataCacheGroup dest("metadata_cache_name", "replicaset_name", "read-only",
                                uri.query, Protocol::Type::kClassicProtocol),
      std::runtime_error,
     "Unsupported metadata-cache parameter in URI: \"xxx\""
    );
  }
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
