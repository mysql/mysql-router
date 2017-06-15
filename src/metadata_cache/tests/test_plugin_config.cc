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

#include <typeinfo>

#include "../src/plugin_config.h"
#include "../src/metadata_cache.h"

#include "gmock/gmock.h"

using ::testing::StrEq;
using ::testing::Eq;
using ::testing::ContainerEq;

// demangle the symbols returned by typeid().name() if needed
//
// typeid() on gcc/clang returns a mangled name, msvc doesn't.
static std::string cxx_demangle_name(const char *mangled) {
#if defined(__GNUC__) && defined(__cplusplus)
  // gcc and clang are mangling the names
  std::shared_ptr<char> demangled_name(abi::__cxa_demangle(mangled, 0, 0, nullptr), [&](char *p){
    if (p) free(p);
  });

  return std::string(demangled_name.get());
#else
  return mangled;
#endif
}


// the Good

struct GoodTestData {
  struct {
    std::map<std::string, std::string> extra_config_lines;
  } input;

  struct {
    std::string user;
    unsigned int ttl;
    std::string metadata_cluster;
    std::vector<mysqlrouter::TCPAddress> bootstrap_addresses;
  } expected;
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
  return os << "user=" << test_data.expected.user << ", "
    << "ttl=" << test_data.expected.ttl << ", "
    << "metadata_cluster=" << test_data.expected.metadata_cluster << ", "
    << "bootstrap_server_addresses=" << test_data.expected.bootstrap_addresses;
}

/**
 * check if valid configs can be parsed
 */
TEST_P(MetadataCachePluginConfigGoodTest, GoodConfigs) {
  GoodTestData test_data = GetParam();

  mysql_harness::Config config;
  mysql_harness::ConfigSection& section = config.add("metadata_cache", "");

  for (const std::pair<std::string, std::string>& pair: test_data.input.extra_config_lines) {
    section.add(pair.first, pair.second);
  }

  MetadataCachePluginConfig plugin_config(&section);

  EXPECT_THAT(plugin_config.user, StrEq(test_data.expected.user));
  EXPECT_THAT(plugin_config.ttl, Eq(test_data.expected.ttl));
  EXPECT_THAT(plugin_config.metadata_cluster, StrEq(test_data.expected.metadata_cluster));
  EXPECT_THAT(plugin_config.bootstrap_addresses, ContainerEq(test_data.expected.bootstrap_addresses));
}

INSTANTIATE_TEST_CASE_P(SomethingUseful, MetadataCachePluginConfigGoodTest,
  ::testing::ValuesIn(std::vector<GoodTestData>({
    // minimal config
    {
      {
        std::map<std::string, std::string>({
          { "user", "foo" }, // required
        })
      },

      {
        "foo",
        metadata_cache::kDefaultMetadataTTL,
        "",
        std::vector<mysqlrouter::TCPAddress>()
      }
    },
    // TTL value can be parsed
    {
      {
        std::map<std::string, std::string>({
          { "user", "foo", }, // required
          { "ttl", "123", },
        })
      },

      {
        "foo",
        123,
        "",
        std::vector<mysqlrouter::TCPAddress>()
      }
    },
    // bootstrap_servers, nicely split into pieces
    {
      {
        std::map<std::string, std::string>({
          { "user", "foo", }, // required
          { "ttl", "123", },
          { "bootstrap_server_addresses", "mysql://foobar,mysql://fuzzbozz", },
        })
      },
      {
        "foo",
        123,
        "",
        std::vector<mysqlrouter::TCPAddress>({
          { mysqlrouter::TCPAddress("foobar", metadata_cache::kDefaultMetadataPort), },
          { mysqlrouter::TCPAddress("fuzzbozz", metadata_cache::kDefaultMetadataPort), },
        })
      }
    },
    // bootstrap_servers, single value
    {
      {
        std::map<std::string, std::string>({
          { "user", "foo", }, // required
          { "bootstrap_server_addresses", "mysql://foobar", },
        })
      },

      {
        "foo",
        metadata_cache::kDefaultMetadataTTL,
        "",
        std::vector<mysqlrouter::TCPAddress>({
          { mysqlrouter::TCPAddress("foobar", metadata_cache::kDefaultMetadataPort), },
        })
      }
    },
    // metadata_cluster
    {
      {
        std::map<std::string, std::string>({
          { "user", "foo", }, // required
          { "ttl", "123", },
          { "bootstrap_server_addresses", "mysql://foobar,mysql://fuzzbozz", },
          { "metadata_cluster", "whatisthis", },
        })
      },

      {
        "foo",
        123,
        "whatisthis",
        std::vector<mysqlrouter::TCPAddress>({
          { mysqlrouter::TCPAddress("foobar", metadata_cache::kDefaultMetadataPort), },
          { mysqlrouter::TCPAddress("fuzzbozz", metadata_cache::kDefaultMetadataPort), },
        })
      }
    },
  })));


// the Bad
struct BadTestData {
  struct {
    std::map<std::string, std::string> extra_config_lines;
  } input;

  struct {
    const std::type_info &exception_type;
    std::string exception_msg;
  } expected;
};

class MetadataCachePluginConfigBadTest : public ::testing::Test,
  public ::testing::WithParamInterface<BadTestData> {
};

std::ostream& operator<<(std::ostream& os, const BadTestData& test_data) {
  return os << test_data.expected.exception_type.name();
}

/**
 * check if invalid configs fail properly
 */
TEST_P(MetadataCachePluginConfigBadTest, BadConfigs) {
  BadTestData test_data = GetParam();

  mysql_harness::Config config;
  mysql_harness::ConfigSection& section = config.add("metadata_cache", "");

  for (const std::pair<std::string, std::string>& pair: test_data.input.extra_config_lines) {
    section.add(pair.first, pair.second);
  }

  try {
    MetadataCachePluginConfig plugin_config(&section);
    FAIL() << "should have failed";
  } catch (const std::exception &exc) {
    EXPECT_THAT(cxx_demangle_name(typeid(exc).name()), StrEq(cxx_demangle_name(test_data.expected.exception_type.name())));
    EXPECT_THAT(exc.what(), StrEq(test_data.expected.exception_msg));
  }
}

INSTANTIATE_TEST_CASE_P(SomethingUseful, MetadataCachePluginConfigBadTest,
  ::testing::ValuesIn(std::vector<BadTestData>({
    // user option is required
    {
      {
        std::map<std::string, std::string>(),
      },

      {
        typeid(std::invalid_argument),
        "option user in [metadata_cache] is required",
      }
    },
    // ttl is garbage
    {
      {
        std::map<std::string, std::string>({
          { "user", "foo" }, // required
          { "ttl", "garbage" },
        }),
      },

      {
        typeid(std::invalid_argument),
        "option ttl in [metadata_cache] needs value between 0 and 4294967295 inclusive, was 'garbage'",
      }
    },
  })));
