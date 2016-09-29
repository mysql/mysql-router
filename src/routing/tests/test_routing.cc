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

#include "mysqlrouter/routing.h"
#include "mysql_routing.h"

#include "routing_mocks.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#ifdef __sun
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#endif

#ifdef __sun
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif

using routing::AccessMode;
using routing::set_socket_blocking;

using ::testing::ContainerEq;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

class RoutingTests : public ::testing::Test {
protected:
  RoutingTests() {
  }
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  MockSocketOperations socket_op;
};

TEST_F(RoutingTests, AccessModes) {
  ASSERT_EQ(static_cast<int>(AccessMode::kReadWrite), 1);
  ASSERT_EQ(static_cast<int>(AccessMode::kReadOnly), 2);
}

TEST_F(RoutingTests, AccessModeLiteralNames) {
  using routing::get_access_mode;
  ASSERT_THAT(get_access_mode("read-write"), Eq(AccessMode::kReadWrite));
  ASSERT_THAT(get_access_mode("read-only"), Eq(AccessMode::kReadOnly));
}

TEST_F(RoutingTests, GetAccessLiteralName) {
  using routing::get_access_mode_name;
  ASSERT_THAT(get_access_mode_name(AccessMode::kReadWrite), StrEq("read-write"));
  ASSERT_THAT(get_access_mode_name(AccessMode::kReadOnly), StrEq("read-only"));
}

TEST_F(RoutingTests, Defaults) {
  ASSERT_EQ(routing::kDefaultWaitTimeout, 0);
  ASSERT_EQ(routing::kDefaultMaxConnections, 512);
  ASSERT_EQ(routing::kDefaultDestinationConnectionTimeout, 1);
  ASSERT_EQ(routing::kDefaultBindAddress, "127.0.0.1");
  ASSERT_EQ(routing::kDefaultNetBufferLength, 16384U);
  ASSERT_EQ(routing::kDefaultMaxConnectErrors, 100ULL);
  ASSERT_EQ(routing::kDefaultClientConnectTimeout, 9UL);
}

#ifndef _WIN32
// No way to read nonblocking status in Windows
TEST_F(RoutingTests, SetSocketBlocking) {
  int s = socket(PF_INET, SOCK_STREAM, 6);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_NONBLOCK, 0);
  set_socket_blocking(s, false);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_NONBLOCK, O_NONBLOCK);
  set_socket_blocking(s, true);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_NONBLOCK, 0) << std::endl;

  fcntl(s, F_SETFL, O_RDONLY);
  set_socket_blocking(s, false);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_NONBLOCK, O_NONBLOCK);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_RDONLY, O_RDONLY);
}
#endif

TEST_F(RoutingTests, CopyPacketsSingleWrite) {
  int sender_socket = 1, receiver_socket = 2;
  mysql_protocol::Packet::vector_t buffer(500);
  fd_set readfds;
  int curr_pktnr = 100;
  bool handshake_done = true;
  size_t report_bytes_read = 0u;

  FD_ZERO(&readfds);
  FD_SET(sender_socket, &readfds);
  FD_SET(receiver_socket, &readfds);

  EXPECT_CALL(socket_op, read(sender_socket, &buffer[0], buffer.size())).WillOnce(Return(200));
  EXPECT_CALL(socket_op, write(receiver_socket, &buffer[0], 200)).WillOnce(Return(200));

  int res = MySQLRouting::copy_mysql_protocol_packets(1, 2, &readfds,
                                  buffer, &curr_pktnr,
                                  handshake_done, &report_bytes_read,
                                  &socket_op);

  ASSERT_EQ(0, res);
  ASSERT_EQ(200u, report_bytes_read);
}

TEST_F(RoutingTests, CopyPacketsMultipleWrites) {
  int sender_socket = 1, receiver_socket = 2;
  mysql_protocol::Packet::vector_t buffer(500);
  fd_set readfds;
  int curr_pktnr = 100;
  bool handshake_done = true;
  size_t report_bytes_read = 0u;

  FD_ZERO(&readfds);
  FD_SET(sender_socket, &readfds);
  FD_SET(receiver_socket, &readfds);

  InSequence seq;

  EXPECT_CALL(socket_op, read(sender_socket, &buffer[0], buffer.size())).WillOnce(Return(200));

  // first write does not write everything
  EXPECT_CALL(socket_op, write(receiver_socket, &buffer[0], 200)).WillOnce(Return(100));
  // second does not do anything (which is not treated as an error
  EXPECT_CALL(socket_op, write(receiver_socket, &buffer[100], 100)).WillOnce(Return(0));
  // third writes the remaining chunk
  EXPECT_CALL(socket_op, write(receiver_socket, &buffer[100], 100)).WillOnce(Return(100));

  int res = MySQLRouting::copy_mysql_protocol_packets(1, 2, &readfds,
                                  buffer, &curr_pktnr,
                                  handshake_done, &report_bytes_read,
                                  &socket_op);

  ASSERT_EQ(0, res);
  ASSERT_EQ(200u, report_bytes_read);
}

TEST_F(RoutingTests, CopyPacketsWriteError) {
  int sender_socket = 1, receiver_socket = 2;
  mysql_protocol::Packet::vector_t buffer(500);
  fd_set readfds;
  int curr_pktnr = 100;
  bool handshake_done = true;
  size_t report_bytes_read = 0u;

  FD_ZERO(&readfds);
  FD_SET(sender_socket, &readfds);
  FD_SET(receiver_socket, &readfds);

  EXPECT_CALL(socket_op, read(sender_socket, &buffer[0], buffer.size())).WillOnce(Return(200));
  EXPECT_CALL(socket_op, write(receiver_socket, &buffer[0], 200)).WillOnce(Return(-1));

  int res = MySQLRouting::copy_mysql_protocol_packets(1, 2, &readfds,
                                  buffer, &curr_pktnr,
                                  handshake_done, &report_bytes_read,
                                  &socket_op);

  ASSERT_EQ(-1, res);
}
