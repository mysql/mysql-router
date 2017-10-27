/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "cmd_exec.h"
#include "gtest_consoleoutput.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/plugin.h"
#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/routing.h"
#include "router_test_helpers.h"
#include "../../router/src/router_app.h"
#include "config_parser.h"
#include "mysql/harness/plugin.h"
#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/routing.h"
#include "../../routing/src/mysql_routing.h"
#include "../../routing/src/utils.h"

#include <cstdio>
#include <fstream>
#include <future>
#include <memory>
#include <string>
#include <thread>
#ifndef _WIN32
# include <netinet/in.h>
#else
# include <WinSock2.h>
#endif

#include "gmock/gmock.h"

using std::string;
using ::testing::ContainerEq;
using ::testing::HasSubstr;
using ::testing::StrEq;
using mysql_harness::Path;

string g_cwd;
Path g_origin;

class TestBlockClients : public ConsoleOutputTest {
protected:
  virtual void SetUp() {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
  }
};

TEST_F(TestBlockClients, BlockClientHost) {
  unsigned long long max_connect_errors = 2;
  std::chrono::seconds client_connect_timeout(2);
  sockaddr_in6 client_addr1, client_addr2;
  client_addr1.sin6_family = client_addr2.sin6_family = AF_INET6;
  memset(&client_addr1.sin6_addr, 0x0, sizeof(client_addr1.sin6_addr));
  memset(&client_addr2.sin6_addr, 0x0, sizeof(client_addr2.sin6_addr));
  unsigned char* p1 = reinterpret_cast<unsigned char*>(&client_addr1.sin6_addr);
  p1[15] = 1;
  unsigned char* p2 = reinterpret_cast<unsigned char*>(&client_addr2.sin6_addr);
  p2[15] = 2;

  auto client_ip_array1 = in_addr_to_array(*reinterpret_cast<sockaddr_storage*>(&client_addr1));
  auto client_ip_array2 = in_addr_to_array(*reinterpret_cast<sockaddr_storage*>(&client_addr2));

  MySQLRouting r(routing::AccessMode::kReadWrite, 7001, Protocol::Type::kClassicProtocol,
                 "127.0.0.1", mysql_harness::Path(), "routing:connect_erros",
                 1, std::chrono::seconds(1), max_connect_errors, client_connect_timeout);

  ASSERT_FALSE(r.block_client_host(client_ip_array1, string("::1")));
  ASSERT_THAT(ssout.str(), HasSubstr("1 connection errors for ::1 (max 2)"));
  reset_ssout();
  ASSERT_TRUE(r.block_client_host(client_ip_array1, string("::1")));
  ASSERT_THAT(ssout.str(), HasSubstr("blocking client host ::1"));

  auto blocked_hosts = r.get_blocked_client_hosts();
  ASSERT_GE(blocked_hosts.size(), 1u);
  ASSERT_THAT(blocked_hosts[0], ContainerEq(client_ip_array1));

  ASSERT_FALSE(r.block_client_host(client_ip_array2, string("::2")));
  ASSERT_TRUE(r.block_client_host(client_ip_array2, string("::2")));

  blocked_hosts = r.get_blocked_client_hosts();
  ASSERT_THAT(blocked_hosts[0], ContainerEq(client_ip_array1));
  ASSERT_THAT(blocked_hosts[1], ContainerEq(client_ip_array2));
}

TEST_F(TestBlockClients, BlockClientHostWithFakeResponse) {
  unsigned long long max_connect_errors = 2;
  std::chrono::seconds client_connect_timeout(2);
  sockaddr_in6 client_addr1;
  client_addr1.sin6_family = AF_INET6;
  memset(&client_addr1.sin6_addr, 0x0, sizeof(client_addr1.sin6_addr));
  unsigned char* p = reinterpret_cast<unsigned char*>(&client_addr1.sin6_addr);
  p[15] = 1;
  auto client_ip_array1 = in_addr_to_array(*reinterpret_cast<sockaddr_storage*>(&client_addr1));

  MySQLRouting r(routing::AccessMode::kReadWrite, 7001, Protocol::Type::kClassicProtocol,
                 "127.0.0.1", mysql_harness::Path(), "routing:connect_erros",
                 1, std::chrono::seconds(1), max_connect_errors, client_connect_timeout);

  std::FILE* fd_response = std::fopen("fake_response.data", "w");

  ASSERT_FALSE(r.block_client_host(client_ip_array1, string("::1"), fileno(fd_response)));
  std::fclose(fd_response);
#ifndef _WIN32
  // block_client_host() will not be able to write data to the file because in windows, the
  // syscall to writing to sockets is different than for files
  fd_response = std::fopen("fake_response.data", "r");

  auto fake_response = mysql_protocol::HandshakeResponsePacket(1, {}, "ROUTER", "", "fake_router_login");

  auto server_response = ssout.str();
  for (size_t i = 0; i < fake_response.size(); ++i) {
     ASSERT_EQ(fake_response.at(i), std::fgetc(fd_response));
  }
  std::fclose(fd_response);
#endif
  std::remove("fake_response.data");
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
