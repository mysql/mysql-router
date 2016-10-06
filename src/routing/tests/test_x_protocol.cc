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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <memory>
#include <google/protobuf/io/coded_stream.h>

#include "logger.h"
#include "protocol/x_protocol.h"
#include "mysqlrouter/routing.h"
#include "routing_mocks.h"
#include "mysqlx.pb.h"

using ::testing::_;
using ::testing::Return;


class XProtocolTest : public ::testing::Test {
protected:
  XProtocolTest() {
    mock_socket_operations_.reset(new MockSocketOperations());
    x_protocol_.reset(new XProtocol(mock_socket_operations_.get()));
  }

  virtual void SetUp() {
    FD_ZERO(&readfds_);

    network_buffer_.resize(routing::kDefaultNetBufferLength);
    network_buffer_offset_ = 0;
    curr_pktnr_ = 0;
    handshake_done_ = false;
  }

  std::unique_ptr<MockSocketOperations> mock_socket_operations_;
  std::unique_ptr<BaseProtocol> x_protocol_;

  void serialize_protobuf_msg_to_buffer(RoutingProtocolBuffer& buffer,
                                        size_t &buffer_offset,
                                        google::protobuf::Message& msg,
                                        unsigned char type) {
    size_t msg_size = msg.ByteSize();
    google::protobuf::io::CodedOutputStream::WriteLittleEndian32ToArray(static_cast<uint32_t>(msg_size + 1),
                                                                        &buffer[buffer_offset]);
    buffer[buffer_offset+4] = type;
    bool res = msg.SerializeToArray(&buffer[buffer_offset+5], msg.ByteSize());
    buffer_offset += (msg_size+5);
    ASSERT_TRUE(res);
  }

  const int sender_socket_ = 1;
  const int receiver_socket_ = 2;

  fd_set readfds_;
  RoutingProtocolBuffer network_buffer_;
  size_t network_buffer_offset_;
  int curr_pktnr_;
  bool handshake_done_;
};

static Mysqlx::Connection::CapabilitiesSet create_capabilities_message(
                        Mysqlx::Datatypes::Scalar_Type type =
                          Mysqlx::Datatypes::Scalar_Type_V_BOOL) {
  Mysqlx::Connection::CapabilitiesSet result;

  Mysqlx::Connection::Capability *capability = result.mutable_capabilities()->add_capabilities();
  capability->set_name("tls");
  capability->mutable_value()->set_type(Mysqlx::Datatypes::Any_Type_SCALAR);
  capability->mutable_value()->mutable_scalar()->set_type(type);

  switch (type) {
  case Mysqlx::Datatypes::Scalar_Type_V_UINT:
      capability->mutable_value()->mutable_scalar()->set_v_unsigned_int(1);
      break;
  case Mysqlx::Datatypes::Scalar_Type_V_SINT:
    capability->mutable_value()->mutable_scalar()->set_v_signed_int(1);
    break;
  default:
    capability->mutable_value()->mutable_scalar()->set_v_bool(true);
  }

  return result;
}


TEST_F(XProtocolTest, OnBlockClientHost)
{
  // currently does nothing
  x_protocol_->on_block_client_host(1, "routing");
}

TEST_F(XProtocolTest, CopyPacketsFdNotSet)
{
  size_t report_bytes_read = 0xff;

  FD_CLR(sender_socket_, &readfds_);

  int result = x_protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, true);

  ASSERT_TRUE(result==0);
  ASSERT_TRUE(report_bytes_read==0);
  ASSERT_FALSE(handshake_done_);
}

TEST_F(XProtocolTest, CopyPacketsReadError)
{
  size_t report_bytes_read = 0xff;

  FD_SET(sender_socket_, &readfds_);

  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_,_,_)).WillOnce(Return(-1));

  int result = x_protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, true);

  ASSERT_FALSE(handshake_done_);
  ASSERT_EQ(-1, result);
}

TEST_F(XProtocolTest, CopyPacketsHandshakeDoneOK)
{
  handshake_done_ = true;
  size_t report_bytes_read = 0xff;
  const int MSG_SIZE = 20;

  FD_SET(sender_socket_, &readfds_);

  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, &network_buffer_[0], network_buffer_.size())).WillOnce(Return(MSG_SIZE));
  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, &network_buffer_[0], 20)).WillOnce(Return(MSG_SIZE));

  int result = x_protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, true);

  ASSERT_TRUE(handshake_done_);
  ASSERT_EQ(0, result);
  ASSERT_EQ(static_cast<size_t>(MSG_SIZE), report_bytes_read);
}

TEST_F(XProtocolTest, CopyPacketsHandshakeDoneWriteError)
{
  handshake_done_ = true;
  size_t report_bytes_read = 0xff;
  const ssize_t MSG_SIZE = 20;

  FD_SET(sender_socket_, &readfds_);

  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, &network_buffer_[0], network_buffer_.size())).
                                                                  WillOnce(Return(MSG_SIZE));
  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, &network_buffer_[0], 20)).WillOnce(Return(-1));

  int result = x_protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, true);

  ASSERT_TRUE(handshake_done_);
  ASSERT_EQ(-1, result);
}

TEST_F(XProtocolTest, CopyPacketsHandshakeSSLEnable)
{
  size_t report_bytes_read = 0xff;

  Mysqlx::Connection::CapabilitiesSet capabilites_msg = create_capabilities_message();

  serialize_protobuf_msg_to_buffer(network_buffer_, network_buffer_offset_, capabilites_msg,
                                   Mysqlx::ClientMessages::CON_CAPABILITIES_SET);

  FD_SET(sender_socket_, &readfds_);
  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, &network_buffer_[0], network_buffer_.size())).
                                                                              WillOnce(Return(network_buffer_offset_));
  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, &network_buffer_[0], network_buffer_offset_)).
                                                                              WillOnce(Return(network_buffer_offset_));

  int result = x_protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, false);

  // during the handshake the client has requested SSL
  // so we should have set handshake_done_ flag
  ASSERT_TRUE(handshake_done_);
  ASSERT_EQ(0, result);
  ASSERT_EQ(network_buffer_offset_, report_bytes_read);
}

TEST_F(XProtocolTest, CopyPacketsHandshakeAuthenticationOk)
{
  size_t report_bytes_read = 0xff;

  Mysqlx::Session::AuthenticateOk authok_msg;

  serialize_protobuf_msg_to_buffer(network_buffer_, network_buffer_offset_, authok_msg,
                                   Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK);

  FD_SET(sender_socket_, &readfds_);
  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, &network_buffer_[0], network_buffer_.size())).
                                                                              WillOnce(Return(network_buffer_offset_));
  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, &network_buffer_[0], network_buffer_offset_)).
                                                                              WillOnce(Return(network_buffer_offset_));

  int result = x_protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, true);

  // server is confirming authentication
  // so we should have set handshake_done_ flag
  ASSERT_TRUE(handshake_done_);
  ASSERT_EQ(0, result);
  ASSERT_EQ(network_buffer_offset_, report_bytes_read);
}

TEST_F(XProtocolTest, CopyPacketsHandshakeOtherMessage)
{
  size_t report_bytes_read = 0xff;

  Mysqlx::Session::AuthenticateContinue autcont_msg;
  autcont_msg.set_auth_data("auth_data");

  serialize_protobuf_msg_to_buffer(network_buffer_, network_buffer_offset_, autcont_msg,
                                   Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE);

  FD_SET(sender_socket_, &readfds_);
  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, &network_buffer_[0], network_buffer_.size())).
                                                                          WillOnce(Return(network_buffer_offset_));
  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, &network_buffer_[0], network_buffer_offset_)).
                                                                          WillOnce(Return(network_buffer_offset_));

  int result = x_protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, true);

  // handshake_done_ should stay untouched
  ASSERT_FALSE(handshake_done_);
  ASSERT_EQ(0, result);
  ASSERT_EQ(network_buffer_offset_, report_bytes_read);
}

TEST_F(XProtocolTest, CopyPacketsHandshakeOneReadTwoMessages)
{
  size_t report_bytes_read = 0xff;

  Mysqlx::Notice::Frame notice_msg;
  notice_msg.set_type(0);
  notice_msg.set_payload("notice payload");
  Mysqlx::Connection::CapabilitiesSet capabilites_msg = create_capabilities_message();

  serialize_protobuf_msg_to_buffer(network_buffer_, network_buffer_offset_, notice_msg,
                                   Mysqlx::ServerMessages::NOTICE);
  serialize_protobuf_msg_to_buffer(network_buffer_, network_buffer_offset_, capabilites_msg,
                                   Mysqlx::ClientMessages::CON_CAPABILITIES_SET);

  FD_SET(sender_socket_, &readfds_);
  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, &network_buffer_[0], network_buffer_.size())).
                                                                  WillOnce(Return(network_buffer_offset_));
  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, &network_buffer_[0], network_buffer_offset_)).
                                                                  WillOnce(Return(network_buffer_offset_));

  int result = x_protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, false);

  // handshake_done_ should be set after the second message
  ASSERT_TRUE(handshake_done_);
  ASSERT_EQ(0, result);
  ASSERT_EQ(network_buffer_offset_, report_bytes_read);
}

TEST_F(XProtocolTest, CopyPacketsHandshakeReadPartialMessage)
{
  size_t report_bytes_read = 0xff;

  Mysqlx::Connection::CapabilitiesSet capabilites_msg = create_capabilities_message();

  serialize_protobuf_msg_to_buffer(network_buffer_, network_buffer_offset_, capabilites_msg,
                                   Mysqlx::ClientMessages::CON_CAPABILITIES_SET);

  FD_SET(sender_socket_, &readfds_);
  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, _, _)).
                                               Times(2).
                                               WillOnce(Return(network_buffer_offset_-8)).
                                               WillOnce(Return(8));
  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, &network_buffer_[0], network_buffer_offset_)).
                                                WillOnce(Return(network_buffer_offset_));

  int result = x_protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, false);

  // handshake_done_ should bet set
  ASSERT_TRUE(handshake_done_);
  ASSERT_EQ(0, result);
  ASSERT_EQ(network_buffer_offset_, report_bytes_read);
}

TEST_F(XProtocolTest, CopyPacketsHandshakeCapabilityAsSignedInteger)
{
  size_t report_bytes_read = 0xff;

  Mysqlx::Connection::CapabilitiesSet capabilites_msg =
      create_capabilities_message(Mysqlx::Datatypes::Scalar_Type_V_SINT);

  serialize_protobuf_msg_to_buffer(network_buffer_, network_buffer_offset_, capabilites_msg,
                                   Mysqlx::ClientMessages::CON_CAPABILITIES_SET);

  FD_SET(sender_socket_, &readfds_);
  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, _, _)).
                                               WillOnce(Return(network_buffer_offset_));
  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, &network_buffer_[0], network_buffer_offset_)).
                                                WillOnce(Return(network_buffer_offset_));

  int result = x_protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, false);

  // handshake_done_ should bet set
  ASSERT_TRUE(handshake_done_);
  ASSERT_EQ(0, result);
  ASSERT_EQ(network_buffer_offset_, report_bytes_read);
}

TEST_F(XProtocolTest, CopyPacketsHandshakeCapabilityAsUnsignedInteger)
{
  size_t report_bytes_read = 0xff;

  Mysqlx::Connection::CapabilitiesSet capabilites_msg =
      create_capabilities_message(Mysqlx::Datatypes::Scalar_Type_V_UINT);

  serialize_protobuf_msg_to_buffer(network_buffer_, network_buffer_offset_, capabilites_msg,
                                   Mysqlx::ClientMessages::CON_CAPABILITIES_SET);

  FD_SET(sender_socket_, &readfds_);
  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, _, _)).
                                               WillOnce(Return(network_buffer_offset_));
  EXPECT_CALL(*mock_socket_operations_, write(receiver_socket_, &network_buffer_[0], network_buffer_offset_)).
                                                WillOnce(Return(network_buffer_offset_));

  int result = x_protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, false);

  // handshake_done_ should bet set
  ASSERT_TRUE(handshake_done_);
  ASSERT_EQ(0, result);
  ASSERT_EQ(network_buffer_offset_, report_bytes_read);
}

TEST_F(XProtocolTest, CopyPacketsHandshakeMsgBiggerThanBuffer)
{
  size_t report_bytes_read = 0xff;

  Mysqlx::Connection::CapabilitiesSet capabilites_msg;
  // make the message bigger than the current network buffer size
  for (size_t i = 0; ; ++i) {
    Mysqlx::Connection::Capability *capability = capabilites_msg.mutable_capabilities()->add_capabilities();
    std::string name{"quite_loong_descriptive_name_of_the_capability_number_" + std::to_string(i)};
    capability->set_name(name);
    capability->mutable_value()->set_type(Mysqlx::Datatypes::Any_Type_SCALAR);
    capability->mutable_value()->mutable_scalar()->set_type(Mysqlx::Datatypes::Scalar_Type_V_UINT);
    capability->mutable_value()->mutable_scalar()->set_v_unsigned_int(i);

    if ((unsigned)capabilites_msg.ByteSize() > 2*routing::kDefaultNetBufferLength) {
      break;
    }
  }

  RoutingProtocolBuffer msg_buffer(capabilites_msg.ByteSize()+5);
  const auto BUFFER_SIZE = network_buffer_.size();

  serialize_protobuf_msg_to_buffer(msg_buffer, network_buffer_offset_, capabilites_msg,
                                   Mysqlx::ClientMessages::CON_CAPABILITIES_SET);

  // copy part of the message to the network buffer
  std::copy(msg_buffer.begin(), msg_buffer.begin()+network_buffer_.size(), network_buffer_.begin());

  FD_SET(sender_socket_, &readfds_);
  EXPECT_CALL(*mock_socket_operations_, read(sender_socket_, _, _)).Times(1).
                                             WillOnce(Return(network_buffer_.size()));

  int result = x_protocol_->copy_packets(sender_socket_, receiver_socket_, &readfds_, network_buffer_, &curr_pktnr_,
                                         handshake_done_, &report_bytes_read, false);

  // the size of buffer passed to copy_packets should be untouched
  ASSERT_EQ(BUFFER_SIZE, network_buffer_.size());
  ASSERT_FALSE(handshake_done_);
  ASSERT_EQ(-1, result);
}

TEST_F(XProtocolTest, SendErrorOKMultipleWrites)
{
  EXPECT_CALL(*mock_socket_operations_, write(1, _, _)).Times(2).
                                                  WillOnce(Return(8)).
                                                  WillOnce(Return  (10000));

  bool res = x_protocol_->send_error(1, 55, "Error message", "SQL_STATE",
                                     "routing configuration name");

  ASSERT_TRUE(res);
}

TEST_F(XProtocolTest, SendErrorWriteFail)
{
  EXPECT_CALL(*mock_socket_operations_, write(1, _, _)).WillOnce(Return(-1));

  bool res = x_protocol_->send_error(1, 55, "Error message", "SQL_STATE",
                                     "routing configuration name");

  ASSERT_FALSE(res);
}
