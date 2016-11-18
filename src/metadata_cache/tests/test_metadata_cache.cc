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
 * Test the metadata cache implementation.
 */

#include "metadata_cache.h"
#include "metadata_factory.h"
#include "mock_metadata.h"
#include "cluster_metadata.h"

#include "gmock/gmock.h"

#include "mysqlrouter/datatypes.h"

using metadata_cache::ManagedInstance;

class MetadataCacheTest : public ::testing::Test {
public:
  MockNG mf;
  MetadataCache cache;

  MetadataCacheTest() : mf("admin", "admin", 1, 1, 10),
                      cache({mysqlrouter::TCPAddress("localhost", 32275)},
                              get_instance("admin", "admin", 1, 1, 10),
                              10, "replicaset-1") {}
};

/**
 * Test that the list of servers that are part of a replicaset is accurate.
 */
TEST_F(MetadataCacheTest, ValidReplicasetTest_1) {
  std::vector<ManagedInstance> instance_vector_1;

  instance_vector_1 = cache.replicaset_lookup("replicaset-1");
  ASSERT_EQ(3U, instance_vector_1.size());
  EXPECT_EQ(instance_vector_1[0], mf.ms1);
  EXPECT_EQ(instance_vector_1[1], mf.ms2);
  EXPECT_EQ(instance_vector_1[2], mf.ms3);
}

/**
 * Test that looking up an invalid replicaset returns a empty list.
 */
TEST_F(MetadataCacheTest, InvalidReplicasetTest) {
  std::vector<ManagedInstance> instance_vector;

  instance_vector = cache.replicaset_lookup("InvalidReplicasetTest");

  EXPECT_TRUE(instance_vector.empty());
}
