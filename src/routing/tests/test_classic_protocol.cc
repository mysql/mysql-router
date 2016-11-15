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

#include <memory>

#include "logger.h"
#include "protocol/classic_protocol.h"
#include "mysqlrouter/routing.h"
#include "routing_mocks.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;


class ClassicProtocolTest : public ::testing::Test {
protected:
  ClassicProtocolTest() {
    mock_socket_operations_.reset(new MockSocketOperations());
    protocol_.reset(new ClassicProtocol(mock_socket_operations_.get()));
  }

  virtual void SetUp() {
    FD_ZERO(&readfds_);

    network_buffer_.resize(routing::kDefaultNetBufferLength);
    network_buffer_offset_ = 0;
    curr_pktnr_ = 0;
    handshake_done_ = false;
  }

  std::unique_ptr<MockSocketOperations> mock_socket_operations_;
  std::unique_ptr<BaseProtocol> protocol_;

  void serialize_classic_packet_to_buffer(RoutingProtocolBuffer &buffer,
                                          size_t &buffer_offset,
                                          const mysql_protocol::Packet &packet
                                          ) {
    using diff_t = mysql_protocol::Packet::difference_type;
    std::copy(packet.begin(), packet.begin()+static_cast<diff_t>(packet.size()),
              buffer.begin()+static_cast<diff_t>(buffer_offset));
    buffer_offset += packet.size();
  }

  static constexpr int sender_socket_ = 1;
  static constexpr int receiver_socket_ = 2;

  fd_set readfds_;
  RoutingProtocolBuffer network_buffer_;
  size_t network_buffer_offset_;
  int curr_pktnr_;
  bool handshake_done_;
};

TEST_F(ClassicProtocolTest, OnBlockClientHostSuccess)
{
  // we expect the router sending fake response packet
  // to prevent MySQL server from bumping up connection error counter
  auto packet = mysql_protocol::HandshakeResponsePacket(1, {}, "ROUTER", "", "fake_router_login");

  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, _, packet.size())).WillOnce(Return((ssize_t)packet.size()));

  const bool result = protocol_->on_block_client_host(receiver_socket_, "routing");

  ASSERT_TRUE(result);
}

TEST_F(ClassicProtocolTest, OnBlockClientHostWriteFail)
{
  auto packet = mysql_protocol::HandshakeResponsePacket(1, {}, "ROUTER", "", "fake_router_login");

  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, _, packet.size())).WillOnce(Return(-1));

  const bool result = protocol_->on_block_client_host(receiver_socket_, "routing");

  ASSERT_FALSE(result);
}

TEST_F(ClassicProtocolTest, CopyPacketsFdNotSet)
{
  size_t report_bytes_read = 0xff;

  FD_CLR(sender_socket_, &readfds_);

  int result = protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, true);

  ASSERT_TRUE(result==0);
  ASSERT_TRUE(report_bytes_read==0);
  ASSERT_FALSE(handshake_done_);
}

TEST_F(ClassicProtocolTest, CopyPacketsReadError)
{
  size_t report_bytes_read = 0xff;

  FD_SET(sender_socket_, &readfds_);

  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_,_,_)).WillOnce(Return(-1));

  int result = protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, true);

  ASSERT_FALSE(handshake_done_);
  ASSERT_EQ(-1, result);
}

TEST_F(ClassicProtocolTest, CopyPacketsHandshakeDoneOK)
{
  handshake_done_ = true;
  size_t report_bytes_read = 0xff;
  constexpr int PACKET_SIZE = 20;

  FD_SET(sender_socket_, &readfds_);

  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, &network_buffer_[0], network_buffer_.size())).WillOnce(Return(PACKET_SIZE));
  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, &network_buffer_[0], PACKET_SIZE)).WillOnce(Return(PACKET_SIZE));

  int result = protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, true);

  ASSERT_TRUE(handshake_done_);
  ASSERT_EQ(0, result);
  ASSERT_EQ(static_cast<size_t>(PACKET_SIZE), report_bytes_read);
}

TEST_F(ClassicProtocolTest, CopyPacketsHandshakeDoneWriteError)
{
  handshake_done_ = true;
  size_t report_bytes_read = 0xff;
  constexpr ssize_t PACKET_SIZE = 20;

  FD_SET(sender_socket_, &readfds_);

  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, &network_buffer_[0], network_buffer_.size())).
                                                                  WillOnce(Return(PACKET_SIZE));
  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, &network_buffer_[0], 20)).WillOnce(Return(-1));

  int result = protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                       handshake_done_, &report_bytes_read, true);

  ASSERT_TRUE(handshake_done_);
  ASSERT_EQ(-1, result);
}

TEST_F(ClassicProtocolTest, CopyPacketsHandshakePacketTooSmall)
{
  size_t report_bytes_read = 3;
  FD_SET(sender_socket_, &readfds_);

  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, &network_buffer_[0], network_buffer_.size())).
                                                                  WillOnce(Return((ssize_t)report_bytes_read));

  int result = protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                       handshake_done_, &report_bytes_read, true);

  ASSERT_FALSE(handshake_done_);
  ASSERT_EQ(-1, result);
}

TEST_F(ClassicProtocolTest, CopyPacketsHandshakeInvalidPacketNumber)
{
  size_t report_bytes_read = 0xff;
  FD_SET(sender_socket_, &readfds_);
  constexpr int packet_no = 3;
  curr_pktnr_ = 1;

  auto error_packet = mysql_protocol::ErrorPacket(packet_no, 122, "Access denied", "HY004");
  serialize_classic_packet_to_buffer(network_buffer_, network_buffer_offset_, error_packet);

  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, &network_buffer_[0], network_buffer_.size())).
                                                                  WillOnce(Return((ssize_t)report_bytes_read));

  int result = protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                       handshake_done_, &report_bytes_read, true);

  ASSERT_FALSE(handshake_done_);
  ASSERT_EQ(-1, result);
}


TEST_F(ClassicProtocolTest, CopyPacketsHandshakeServerSendsError)
{
  size_t report_bytes_read = 0xff;
  FD_SET(sender_socket_, &readfds_);
  curr_pktnr_ = 1;

  auto error_packet = mysql_protocol::ErrorPacket(2, 0xaabb, "Access denied", "HY004", mysql_protocol::kClientProtocol41);

  serialize_classic_packet_to_buffer(network_buffer_, network_buffer_offset_, error_packet);

  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, &network_buffer_[0], network_buffer_.size())).
                                                                  WillOnce(Return((ssize_t)network_buffer_offset_));

  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, _, network_buffer_offset_)).
                                                       WillOnce(Return((ssize_t)network_buffer_offset_));

  int result = protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                       handshake_done_, &report_bytes_read, true);

  // if the server sent error handshake is considered done
  ASSERT_EQ(2, curr_pktnr_);
  ASSERT_EQ(0, result);
}

TEST_F(ClassicProtocolTest, SendErrorOKMultipleWrites)
{
  EXPECT_CALL(*mock_socket_operations_, write(1, _, _)).Times(2).
                                                  WillOnce(Return(8)).
                                                  WillOnce(Return (10000));

  bool res = protocol_->send_error(1, 55, "Error message", "HY000",
                                   "routing configuration name");

  ASSERT_TRUE(res);
}

TEST_F(ClassicProtocolTest, SendErrorWriteFail)
{
  auto set_errno = [&]() -> void {errno=15;};
  EXPECT_CALL(*mock_socket_operations_, write(1, _, _)).WillOnce(DoAll(InvokeWithoutArgs(set_errno), Return(-1)));

  bool res = protocol_->send_error(1, 55, "Error message", "HY000",
                                   "routing configuration name");

  ASSERT_FALSE(res);
}
