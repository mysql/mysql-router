/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "../src/plugin_config.h"
#include "../src/metadata_cache.h"

#include "gmock/gmock.h"

using ::testing::StrEq;
using ::testing::Eq;
using ::testing::ContainerEq;


// the Good

struct GoodTestData {
  std::map<std::string, std::string> extra_config_lines;

  std::string user;
  unsigned int ttl;
  std::string metadata_cluster;
  std::vector<mysqlrouter::TCPAddress> bootstrap_addresses;
};

class MetadataCachePluginConfigGoodTest : public ::testing::Test,
  public ::testing::WithParamInterface<GoodTestData> {
};

namespace mysqlrouter {
  // operator needs to be defined in the namespace of the printed type
  std::ostream& operator<<(std::ostream& os, const TCPAddress& addr) {
    return os << addr.str();
  }
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& container) {
  os << "[";
  bool is_first = true;
  for (auto &it: container) {
    if (!is_first) os << ", ";
    os << it;

    is_first = false;
  }
  os << "]";
  return os;
}

std::ostream& operator<<(std::ostream& os, const GoodTestData& test_data) {
  return os << "user=" << test_data.user << ", "
    << "ttl=" << test_data.ttl << ", "
    << "metadata_cluster=" << test_data.metadata_cluster << ", "
    << "bootstrap_server_addresses=" << test_data.bootstrap_addresses;
}

/**
 * check if valid configs can be parsed
 */
TEST_P(MetadataCachePluginConfigGoodTest, GoodConfigs) {
  GoodTestData expected = GetParam();

  mysql_harness::Config config;
  mysql_harness::ConfigSection& section = config.add("metadata_cache", "");

  std::for_each(expected.extra_config_lines.begin(), expected.extra_config_lines.end(),
    [&section](const std::pair<std::string, std::string>& pair) {
      section.add(pair.first, pair.second);
  });

  MetadataCachePluginConfig plugin_config(&section);

  EXPECT_THAT(plugin_config.user, StrEq(expected.user));
  EXPECT_THAT(plugin_config.ttl, Eq(expected.ttl));
  EXPECT_THAT(plugin_config.metadata_cluster, StrEq(expected.metadata_cluster));
  EXPECT_THAT(plugin_config.bootstrap_addresses, ContainerEq(expected.bootstrap_addresses));
}

INSTANTIATE_TEST_CASE_P(SomethingUseful, MetadataCachePluginConfigGoodTest,
  ::testing::ValuesIn(std::vector<GoodTestData>({
    // minimal config
    {
      std::map<std::string, std::string>({
        { "user", "foo" }, // required
      }),

      "foo",
      metadata_cache::kDefaultMetadataTTL,
      "",
      std::vector<mysqlrouter::TCPAddress>()
    },
    // TTL value can be parsed
    {
      std::map<std::string, std::string>({
        { "user", "foo", }, // required
        { "ttl", "123", },
      }),

      "foo",
      123,
      "",
      std::vector<mysqlrouter::TCPAddress>()
    },
    // bootstrap_servers, nicely split into pieces
    {
      std::map<std::string, std::string>({
        { "user", "foo", }, // required
        { "ttl", "123", },
        { "bootstrap_server_addresses", "mysql://foobar,mysql://fuzzbozz", },
      }),

      "foo",
      123,
      "",
      std::vector<mysqlrouter::TCPAddress>({
        { mysqlrouter::TCPAddress("foobar", metadata_cache::kDefaultMetadataPort), },
        { mysqlrouter::TCPAddress("fuzzbozz", metadata_cache::kDefaultMetadataPort), },
      })
    },
    // bootstrap_servers, single value
    {
      std::map<std::string, std::string>({
        { "user", "foo", }, // required
        { "bootstrap_server_addresses", "mysql://foobar", },
      }),

      "foo",
      metadata_cache::kDefaultMetadataTTL,
      "",
      std::vector<mysqlrouter::TCPAddress>({
        { mysqlrouter::TCPAddress("foobar", metadata_cache::kDefaultMetadataPort), },
      })
    },
    // metadata_cluster
    {
      std::map<std::string, std::string>({
        { "user", "foo", }, // required
        { "ttl", "123", },
        { "bootstrap_server_addresses", "mysql://foobar,mysql://fuzzbozz", },
        { "metadata_cluster", "whatisthis", },
      }),

      "foo",
      123,
      "whatisthis",
      std::vector<mysqlrouter::TCPAddress>({
        { mysqlrouter::TCPAddress("foobar", metadata_cache::kDefaultMetadataPort), },
        { mysqlrouter::TCPAddress("fuzzbozz", metadata_cache::kDefaultMetadataPort), },
      })
    },
  })));


// the Bad
struct BadTestData {
  std::map<std::string, std::string> extra_config_lines;

  std::string exception_msg;
};

class MetadataCachePluginConfigBadTest : public ::testing::Test,
  public ::testing::WithParamInterface<BadTestData> {
};

std::ostream& operator<<(std::ostream& os, const BadTestData& ) {
  return os << "";
}

/**
 * check if valid configs can be parsed
 */
TEST_P(MetadataCachePluginConfigBadTest, BadConfigs) {
  BadTestData expected = GetParam();

  mysql_harness::Config config;
  mysql_harness::ConfigSection& section = config.add("metadata_cache", "");

  std::for_each(expected.extra_config_lines.begin(), expected.extra_config_lines.end(),
    [&section](const std::pair<std::string, std::string>& pair) {
      section.add(pair.first, pair.second);
  });

  try {
    MetadataCachePluginConfig plugin_config(&section);
    FAIL() << "should have failed";
  } catch (const std::invalid_argument &exc) {
    // expected exception
    ASSERT_THAT(exc.what(), StrEq(expected.exception_msg));
  }
}

INSTANTIATE_TEST_CASE_P(SomethingUseful, MetadataCachePluginConfigBadTest,
  ::testing::ValuesIn(std::vector<BadTestData>({
    // user option is required
    {
      std::map<std::string, std::string>(),

      "option user in [metadata_cache] is required"
    },
    // ttl is garbage
    {
      std::map<std::string, std::string>({
        { "user", "foo" }, // required
        { "ttl", "garbage" },
      }),

      "option ttl in [metadata_cache] needs value between 0 and 4294967295 inclusive, was 'garbage'"
    },
  })));
