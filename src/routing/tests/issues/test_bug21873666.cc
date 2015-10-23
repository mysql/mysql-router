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

/**
 * BUG21873666 Correctly using configured values instead of defaults
 *
 */

#include "mysqlrouter/routing.h"
#include "plugin_config.h"

#include <fstream>
#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "config_parser.h"

#include "mysql_routing.h"

using mysqlrouter::to_string;
using ::testing::HasSubstr;

class Bug21771595 : public ::testing::Test {
protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
};

TEST_F(Bug21771595, ConstructorDefaults) {
  MySQLRouting r(routing::AccessMode::kReadOnly, 7001, "127.0.0.1", "test");
  ASSERT_EQ(r.get_destination_connect_timeout(), routing::kDefaultDestinationConnectionTimeout);
  ASSERT_EQ(r.get_max_connections(), routing::kDefaultMaxConnections);
}

TEST_F(Bug21771595, Constructor) {
  auto expect_max_connections = routing::kDefaultMaxConnections - 10;
  auto expect_connect_timeout = routing::kDefaultDestinationConnectionTimeout + 10;

  MySQLRouting r(routing::AccessMode::kReadOnly, 7001, "127.0.0.1", "test",
                 expect_max_connections, expect_connect_timeout);
  ASSERT_EQ(r.get_destination_connect_timeout(), expect_connect_timeout);
  ASSERT_EQ(r.get_max_connections(), expect_max_connections);
}

TEST_F(Bug21771595, GetterSetterDestinationConnectionTimeout) {
  MySQLRouting r(routing::AccessMode::kReadOnly, 7001, "127.0.0.1", "test");
  ASSERT_EQ(r.get_destination_connect_timeout(), routing::kDefaultDestinationConnectionTimeout);
  auto expected = routing::kDefaultDestinationConnectionTimeout + 1;
  ASSERT_EQ(r.set_destination_connect_timeout(expected), expected);
  ASSERT_EQ(r.get_destination_connect_timeout(), expected);
}

TEST_F(Bug21771595, GetterSetterMaxConnections) {
  MySQLRouting r(routing::AccessMode::kReadOnly, 7001, "127.0.0.1", "test");
  ASSERT_EQ(r.get_max_connections(), routing::kDefaultMaxConnections);
  auto expected = routing::kDefaultMaxConnections + 1;
  ASSERT_EQ(r.set_max_connections(expected), expected);
  ASSERT_EQ(r.get_max_connections(), expected);
}

TEST_F(Bug21771595, InvalidSetterDestinationConnectTimeout) {
  MySQLRouting r(routing::AccessMode::kReadOnly, 7001, "127.0.0.1", "test");
  ASSERT_THROW(r.set_destination_connect_timeout(-1), std::invalid_argument);
  ASSERT_THROW(r.set_destination_connect_timeout(UINT16_MAX+1), std::invalid_argument);
  try {
    r.set_destination_connect_timeout(0);
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), HasSubstr(
      "tried to set destination_connect_timeout using invalid value, was '0'"));
  }
  ASSERT_THROW(MySQLRouting(routing::AccessMode::kReadOnly, 7001, "127.0.0.1", "test", 1, -1),
      std::invalid_argument);
}

TEST_F(Bug21771595, InvalidMaxConnections) {
  MySQLRouting r(routing::AccessMode::kReadOnly, 7001, "127.0.0.1", "test");
  ASSERT_THROW(r.set_max_connections(-1), std::invalid_argument);
  ASSERT_THROW(r.set_max_connections(UINT16_MAX+1), std::invalid_argument);
  try {
    r.set_max_connections(0);
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), HasSubstr(
      "tried to set max_connections using invalid value, was '0'"));
  }
  ASSERT_THROW(MySQLRouting(routing::AccessMode::kReadOnly, 7001, "127.0.0.1", "test", 0, 1),
    std::invalid_argument);
}

TEST_F(Bug21771595, InvalidPort) {
  ASSERT_THROW(MySQLRouting(routing::AccessMode::kReadOnly, 99999, "127.0.0.1", "test"), std::invalid_argument);
  ASSERT_THROW(MySQLRouting(routing::AccessMode::kReadOnly, 0, "127.0.0.1", "test"), std::invalid_argument);
  ASSERT_THROW(MySQLRouting(routing::AccessMode::kReadOnly, UINT16_MAX+1, "127.0.0.1", "test"), std::invalid_argument);
  try {
    MySQLRouting r(routing::AccessMode::kReadOnly, -1, "127.0.0.1", "test");
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), HasSubstr("Invalid bind address, was '127.0.0.1', port -1"));
  }
}
