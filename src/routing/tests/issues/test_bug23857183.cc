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
 * BUG23857183 UNREACHABLE DESTINATION CONNECTION REQUESTS
 *             ARE NOT HANDLED PROPERLY IN WINDOWS
 *
 */

#include "helper_logger.h"
#include "mysqlrouter/routing.h"
#include "router_test_helpers.h"

#include "gmock/gmock.h"

#include <chrono>
#include <fstream>
#include <functional>
#include <memory>
#include <signal.h>
#include <string>
#include <thread>
#ifdef _WIN32
#include <WinSock2.h>
#endif

class Bug23857183 : public ::testing::Test {
 protected:
  void connect_to(const mysqlrouter::TCPAddress& address);
};

void Bug23857183::connect_to(const mysqlrouter::TCPAddress& address) {
  const int TIMEOUT = 4; // seconds

  auto start = std::chrono::system_clock::now();

  int server = routing::SocketOperations::instance()->get_mysql_socket(address, TIMEOUT);
  ASSERT_LT(server, 0);

  auto duration = std::chrono::duration_cast<std::chrono::seconds>
    (std::chrono::system_clock::now() - start);
  int duration_seconds = static_cast<int>(duration.count());

  // we are trying to connect to the server on wrong port
  // it should not take the whole TIMEOUT to fail
  ASSERT_LT(duration_seconds, TIMEOUT / 2);
}

TEST_F(Bug23857183, ConnectToServerWrongPort) {
  mysqlrouter::TCPAddress addr("127.0.0.1", 10888);

  connect_to(addr);
}

#if !defined(__APPLE__) && !defined(__sun)
// in darwin and solaris, attempting connection to 127.0.0.11 will fail by timeout
TEST_F(Bug23857183, ConnectToServerWrongIpAndPort) {
  mysqlrouter::TCPAddress addr("127.0.0.11", 10888);

  connect_to(addr);
}
#endif

int main(int argc, char *argv[]) {
  init_log();
  init_windows_sockets();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
