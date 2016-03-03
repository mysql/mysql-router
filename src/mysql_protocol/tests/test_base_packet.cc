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


#include <gmock/gmock.h>

#include <cstdlib>
#include <cstring>
#include <string.h>

#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/utils.h"

using std::string;
using ::testing::ContainerEq;
using ::testing::NotNull;

using mysql_protocol::Packet;

class MySQLProtocolPacketTest : public ::testing::Test {
public:
  Packet::vector_t case1 = {0x04, 0x0, 0x0, 0x01, 't', 'e', 's', 't'};
protected:
  virtual void SetUp() {
  }
};

TEST_F(MySQLProtocolPacketTest, Constructors) {
  {
    auto p = Packet();
    EXPECT_EQ(0, p.get_sequence_id());
    EXPECT_EQ(0U, p.get_capabilities());
    EXPECT_EQ(0UL, p.get_payload_size());
  }

  {
    auto p = Packet(2);
    EXPECT_EQ(2, p.get_sequence_id());
    EXPECT_EQ(0U, p.get_capabilities());
    EXPECT_EQ(0U, p.get_payload_size());
  }

  {
    auto p = Packet(2, mysql_protocol::kClientProtocol41);
    EXPECT_EQ(2, p.get_sequence_id());
    EXPECT_EQ(mysql_protocol::kClientProtocol41, p.get_capabilities());
    EXPECT_EQ(0U, p.get_payload_size());
  }
}

TEST_F(MySQLProtocolPacketTest, CopyConstructor) {
  {
    Packet p({0x1, 0x0, 0x0, 0x9, 0x32});
    Packet p_copy(p);
    ASSERT_EQ(p.size(), p_copy.size());
    ASSERT_EQ(p.get_sequence_id(), p_copy.get_sequence_id());
    ASSERT_EQ(0U, p_copy.get_capabilities());
  }

  {
    Packet p({0x1, 0x0, 0x0, 0x9, 0x32}, mysql_protocol::kClientProtocol41);
    Packet p_copy(p);
    ASSERT_EQ(p.size(), p_copy.size());
    ASSERT_EQ(p.get_sequence_id(), p_copy.get_sequence_id());
    ASSERT_EQ(mysql_protocol::kClientProtocol41, p_copy.get_capabilities());
  }
}

TEST_F(MySQLProtocolPacketTest, CopyAssignment) {
  {
    Packet p({0x1, 0x0, 0x0, 0x9, 0x32});
    Packet p_copy{};
    p_copy = p;
    ASSERT_EQ(p.size(), p_copy.size());
    ASSERT_EQ(p.get_sequence_id(), p_copy.get_sequence_id());
    ASSERT_EQ(p.get_payload_size(), p_copy.get_payload_size());
    ASSERT_EQ(0U, p_copy.get_capabilities());
  }

  {
    Packet p({0x1, 0x0, 0x0, 0x9, 0x32}, mysql_protocol::kClientProtocol41);
    Packet p_copy{};
    p_copy = p;
    ASSERT_EQ(p.size(), p_copy.size());
    ASSERT_EQ(p.get_sequence_id(), p_copy.get_sequence_id());
    ASSERT_EQ(p.get_payload_size(), p_copy.get_payload_size());
    ASSERT_EQ(mysql_protocol::kClientProtocol41, p_copy.get_capabilities());
  }
}

TEST_F(MySQLProtocolPacketTest, MoveConstructor) {
  Packet::vector_t buffer = {0x1, 0x0, 0x0, 0x9, 0x32};
  {
    Packet p(buffer, mysql_protocol::kClientProtocol41);
    Packet q(std::move(p));

    ASSERT_EQ(buffer.size(), q.size());
    ASSERT_EQ(mysql_protocol::kClientProtocol41, q.get_capabilities());
    ASSERT_EQ(9U, q.get_sequence_id());
    ASSERT_EQ(1U, q.get_payload_size());

    // original should be empty and re-set
    ASSERT_EQ(0U, p.size());
    ASSERT_EQ(0U, p.get_capabilities());
    ASSERT_EQ(0U, p.get_sequence_id());
    ASSERT_EQ(0U, p.get_payload_size());
  }
}

TEST_F(MySQLProtocolPacketTest, MoveAssigment) {
  Packet::vector_t buffer = {0x1, 0x0, 0x0, 0x9, 0x32};
  {
    Packet p(buffer, mysql_protocol::kClientProtocol41);
    Packet q{};
    q = std::move(p);

    ASSERT_EQ(buffer.size(), q.size());
    ASSERT_EQ(mysql_protocol::kClientProtocol41, q.get_capabilities());
    ASSERT_EQ(9U, q.get_sequence_id());
    ASSERT_EQ(1U, q.get_payload_size());

    // original should be empty and re-set
    ASSERT_EQ(0U, p.size());
    ASSERT_EQ(0U, p.get_capabilities());
    ASSERT_EQ(0U, p.get_sequence_id());
    ASSERT_EQ(0U, p.get_payload_size());
  }
}


TEST_F(MySQLProtocolPacketTest, ConstructWithBuffer) {
  {
    auto p = Packet(case1);
    ASSERT_THAT(p, ContainerEq(case1));
    ASSERT_EQ(4UL, p.get_payload_size());
    ASSERT_EQ(1UL, p.get_sequence_id());
  }

  {
    Packet::vector_t incomplete = {0x04, 0x0, 0x0};
    auto p = Packet(incomplete);
    ASSERT_THAT(p, ContainerEq(incomplete));
    ASSERT_EQ(0UL, p.get_payload_size());
    ASSERT_EQ(0UL, p.get_sequence_id());
  }
}

TEST_F(MySQLProtocolPacketTest, WriteInt) {
  {
    auto packet = Packet(case1);
    Packet::write_int<uint32_t>(packet, 0, 4, 3);
    ASSERT_THAT(packet, ContainerEq(case1));
  }

  {
    auto packet = Packet(case1);
    auto exp = Packet::vector_t{0x04, 0x0, 0x0, 0x1, 0x83, 0xcf, 0x0, 't'};
    Packet::write_int<uint32_t>(packet, 4, 53123, 3);
    ASSERT_THAT(packet, ContainerEq(exp));
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt1Bytes) {
  {
    Packet p{};
    p.add_int<uint8_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0}));

    p.add_int<uint8_t>(134);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x86}));

    p.add_int<uint8_t>(255);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x86, 0xff}));
  }

  {
    // signed
    Packet p{};
    p.add_int<int8_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0}));

    p.add_int<int8_t>(static_cast<int8_t>(-134));
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x7a}));

    p.add_int<int8_t>(static_cast<int8_t>(-254));
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x7a, 0x02}));
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt2Bytes) {
  {
    Packet p{};
    p.add_int<uint16_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00}));

    p.add_int<uint16_t>(134);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00, 0x86, 0x00}));

    p.add_int<uint16_t>(300);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00, 0x86, 0x00, 0x2c, 0x1}));

    p.add_int<uint16_t>(UINT16_MAX);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00, 0x86, 0x00, 0x2c, 0x1, 0xff, 0xff}));
  }

  {
    // signed
    Packet p{};
    p.add_int<int16_t>(INT16_MIN);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x80}));

    p = {};
    p.add_int<int16_t>(INT16_MAX);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0x7f}));
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt3BytesUnsigned) {
  {
    Packet p;
    p.add_int<uint32_t>(0, 3);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00, 0x00}));
  }

  {
    Packet p;
    p.add_int<uint32_t>(134, 3);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x86, 0x00, 0x00}));
  }

  {
    Packet p;
    p.add_int<uint32_t>(500, 3);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xf4, 0x1, 0x0}));
  }

  {
    Packet p;
    p.add_int<uint32_t>(53123, 3);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x83, 0xcf, 0x0}));
  }

  {
    Packet p;
    p.add_int<uint32_t>((1ULL << (CHAR_BIT * 3)) - 1, 3);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0xff, 0xff}));
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt3BytesSigned) {
  Packet p;
  p.add_int<int32_t>(-8388608, 3);
  ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x00, 0x00, 0x80}));

  p = {};
  p.add_int<int32_t>(-1234567, 3);
  ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x79, 0x29, 0xed}));

  p = {};
  p.add_int<int32_t>(8388607, 3);
  ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0xff, 0x7f}));
}

TEST_F(MySQLProtocolPacketTest, PackInt4ByteUnsigned) {
  {
    Packet p;
    p.add_int<uint32_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00, 0x00, 0x00}));
  }

  {
    Packet p;
    p.add_int<uint32_t>(134);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x86, 0x00, 0x00, 0x00}));
  }

  {
    Packet p;
    p.add_int<uint32_t>(500);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xf4, 0x1, 0x00, 0x00}));
  }

  {
    Packet p;
    p.add_int<uint32_t>(53123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x83, 0xcf, 0x0, 0x0}));
  }

  {
    Packet p;
    p.add_int<uint32_t>(2253123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x43, 0x61, 0x22, 0x0}));
  }

  {
    Packet p;
    p.add_int<uint32_t>(UINT32_MAX);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0xff, 0xff, 0xff}));
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt4ByteSigned) {
  {
    Packet p;
    p.add_int<int32_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00, 0x00, 0x00}));
  }

  {
    Packet p;
    p.add_int<int32_t>(134);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x86, 0x00, 0x00, 0x00}));
  }

  {
    Packet p;
    p.add_int<int32_t>(-500);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0c, 0xfe, 0xff, 0xff}));
  }

  {
    Packet p;
    p.add_int<int32_t>(53123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x83, 0xcf, 0x0, 0x0}));
  }

  {
    Packet p;
    p.add_int<int32_t>(-2253123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xbd, 0x9e, 0xdd, 0xff}));
  }

  {
    Packet p;
    p.add_int<int32_t>(INT32_MIN);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x00, 0x00, 0x00, 0x80}));
  }

  {
    Packet p;
    p.add_int<int32_t>(INT32_MAX);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0xff, 0xff, 0x7f}));
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt8BytesUnsigned) {
  {
    Packet p;
    p.add_int<uint64_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.add_int<uint64_t>(134);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x86, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.add_int<uint64_t>(500);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xf4, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.add_int<uint64_t>(53123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x83, 0xcf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.add_int<uint64_t>(2253123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x43, 0x61, 0x22, 0x0, 0x0, 0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.add_int<uint64_t>(361417177240330563UL);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x43, 0x61, 0x22, 0x1, 0x2, 0x3, 0x4, 0x5}));
  }

  {
    Packet p;
    p.add_int<uint64_t>(4294967295);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0xff, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0}));
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt8BytesSigned) {
  {
    Packet p;
    p.add_int<uint64_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.add_int<int64_t>(134);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x86, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.add_int<int64_t>(-500);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0c, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}));
  }

  {
    Packet p;
    p.add_int<int64_t>(53123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x83, 0xcf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.add_int<int64_t>(-2253123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xbd, 0x9e, 0xdd, 0xff, 0xff, 0xff, 0xff, 0xff}));
  }

  {
    Packet p;
    p.add_int<int64_t>(361417177240330563L);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x43, 0x61, 0x22, 0x1, 0x2, 0x3, 0x4, 0x5}));
  }

  {
    Packet p;
    p.add_int<int64_t>(-361417177240330563L);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xbd, 0x9e, 0xdd, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa}));
  }

  {
    Packet p;
    p.add_int<int64_t>(4294967295);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0xff, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.add_int<int64_t>(-4294967295);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x01, 0x0, 0x0, 0x0, 0xff, 0xff, 0xff, 0xff}));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackUInt8) {
  {
    Packet buf{0x10};
    EXPECT_EQ(16, buf.get_int<uint8_t>(0));
  }
  {
    Packet buf{0x10, 0x20};
    EXPECT_EQ(32, buf.get_int<uint8_t>(1));
  }

  {
    Packet buf{0x10};
    EXPECT_EQ(16, buf.get_int<uint8_t>(0, 1));
  }
  {
    Packet buf{0x10, 0x20};
    EXPECT_EQ(16, buf.get_int<uint8_t>(0, 2));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackUInt16) {
  {
    Packet buf{0x10, 0x00};
    EXPECT_EQ(16, buf.get_int<uint16_t>(0, 2));
  }

  {
    Packet buf{0x10, 0x20};
    EXPECT_EQ(8208, buf.get_int<uint16_t>(0));
  }

  {
    Packet buf{0x10, 0x20, 0x30};
    EXPECT_EQ(8208, buf.get_int<uint16_t>(0, 2));
  }

  {
    Packet buf{0xab, 0xba};
    EXPECT_EQ(47787, buf.get_int<uint16_t>(0));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackInt3Bytes) {

  // unsigned
  {
    Packet buf{0x10, 0x00, 0x00};
    EXPECT_EQ(16U, buf.get_int<uint32_t>(0, 3));
  }

  {
    Packet buf{0x10, 0x20, 0x00};
    EXPECT_EQ(8208U, buf.get_int<uint32_t>(0, 3));
  }

  {
    Packet buf{0x10, 0x20, 0x30};
    EXPECT_EQ(3153936U, buf.get_int<uint32_t>(0, 3));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackInt4bytes) {

  // unsigned
  {
    Packet buf({0x10, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(16U, buf.get_int<uint32_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x00, 0x00}, true);
    EXPECT_EQ(8208U, buf.get_int<uint32_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x30, 0x40}, true);
    EXPECT_EQ(1076895760U, buf.get_int<uint32_t>(0, 4));
  }

  {
    Packet buf({0x10, 0x20, 0x30, 0x40, 0x50}, true);
    EXPECT_EQ(1076895760U, buf.get_int<uint32_t>(0, 4));
  }

  // signed
  {
    Packet buf({0xff, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(-1, buf.get_int<int>(0));
  }

  {
    Packet buf({0xf2, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(-14, buf.get_int<int>(0));
  }

  {
    Packet buf({0xff, 0xff, 0xff, 0xfe}, true);
    EXPECT_EQ(-16777217, buf.get_int<int>(0));
  }

  {
    Packet buf({0xff, 0xff, 0xff, 0x7f}, true);
    EXPECT_EQ(2147483647, buf.get_int<int>(0, 4));
  }

  {
    Packet buf({0x02, 0x00, 0x00, 0x80}, true);
    EXPECT_EQ(-2147483646, buf.get_int<int>(0, 4));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackUInt64) {
  {
    Packet buf({0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(16UL, buf.get_int<uint64_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(8208UL, buf.get_int<uint64_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x30, 0x40, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(1076895760UL, buf.get_int<uint64_t>(0, 8));
  }

  {
    Packet buf({0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(4294967295UL, buf.get_int<uint64_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00, 0x80, 0x90}, true);
    EXPECT_EQ(9223372381529055248UL, buf.get_int<uint64_t>(0));
  }

  {
    Packet buf({0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(18446744073709551615UL, buf.get_int<uint64_t>(0));
  }

}

TEST_F(MySQLProtocolPacketTest, UnpackLenEncodedInt) {

  {
    Packet buf(Packet::vector_t{0xfa}, true);
    EXPECT_EQ(250U, buf.get_lenenc_uint(0));
  }

  {
    Packet buf({0xfc, 0xfb, 0x00}, true);
    EXPECT_EQ(251U, buf.get_lenenc_uint(0));
  }

  {
    Packet buf({0xfd, 0x00, 0x00, 0x01}, true);
    EXPECT_EQ(65536U, buf.get_lenenc_uint(0));
  }

  {
    Packet buf({0xfe, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(16777216U, buf.get_lenenc_uint(0));
  }

  {
    Packet buf({0xfe, 0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00, 0x80, 0x90}, true);
    EXPECT_EQ(9223372381529055248UL, buf.get_lenenc_uint(0));
  }

  {
    Packet buf({0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(ULLONG_MAX, buf.get_lenenc_uint(0));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackString) {
  {
    Packet p({'h', 'a', 'm', 0x0, 's', 'p', 'a', 'm'}, true);
    auto res = p.get_string(0);
    EXPECT_EQ(string("ham"), res);
    res = p.get_string(res.size() + 1UL);
    EXPECT_EQ(string("spam"), res);
    res = p.get_string(0, p.size() + 1UL);
    EXPECT_EQ(string("ham"), res);
  }

  {
    Packet p{};
    EXPECT_EQ(string{}, p.get_string(0));
  }

  {
    Packet p({'h', 'a', 'm', 's', 'p', 'a', 'm'}, true);
    EXPECT_EQ(string("hamspam"), p.get_string(0));
  }

  {
    Packet p({'h', 'a', 'm'}, true);
    EXPECT_EQ(string(""), p.get_string(30));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackStringLengthFixed) {
  Packet p({'h', 'a', 'm', 's', 'p', 'a', 'm'}, true);

  {
    auto res = p.get_string(0, 3);
    EXPECT_EQ(res, string("ham"));
  }

  {
    auto res = p.get_string(0, 2);
    EXPECT_EQ(res, string("ha"));
  }

  {
    auto res = p.get_string(3, 4);
    EXPECT_EQ(res, string("spam"));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackBytesLengthEncoded1Byte) {
  Packet p({0x07, 'h', 'a', 'm', 's', 'p', 'a', 'm', 'f', 'o', 'o'}, true);
  auto res = p.get_lenenc_bytes(0);
  ASSERT_THAT(res, ContainerEq(Packet::vector_t{'h', 'a', 'm', 's', 'p', 'a', 'm'}));
}

TEST_F(MySQLProtocolPacketTest, UnpackStringLengthEncoded3Bytes) {
  size_t length = 316;
  unsigned char filler = 0x4d;

  std::vector<uint8_t> data(length + 3, filler);
  data[0] = 0xfc;
  data[1] = 0x3c;
  data[2] = 0x01;
  Packet p(data, true);

  auto res = p.get_lenenc_bytes(0);
  EXPECT_EQ(res.size(), length);
  EXPECT_EQ(res[0], filler);
  EXPECT_EQ(res[length - 1], filler);
}

TEST_F(MySQLProtocolPacketTest, UnpackStringLengthEncoded8Bytes) {
  size_t length = 16777216;
  unsigned char filler = 0x4d;

  std::vector<uint8_t> data(length + 9, filler);
  std::vector<uint8_t> enc_length{0xfe, 0x0, 0x0, 0x0, 0x01, 0x0, 0x0, 0x0, 0x0};
  std::copy(enc_length.begin(), enc_length.end(), data.begin());
  Packet p(data, true);

  auto res = p.get_lenenc_bytes(0);
  EXPECT_EQ(res.size(), length);
  EXPECT_EQ(res[length - 1], filler);
}

TEST_F(MySQLProtocolPacketTest, UnpackStringLengthEncoded8BytesWithNulByte) {
  size_t length = 16777216;
  unsigned char filler = 0x4d;

  std::vector<uint8_t> data(length + 9, filler);
  std::vector<uint8_t> enc_length{0xfe, 0x0, 0x0, 0x0, 0x01, 0x0, 0x0, 0x0, 0x0};
  std::copy(enc_length.begin(), enc_length.end(), data.begin());
  data[length / 2] = 0x0;
  Packet p(data, true);

  auto res = p.get_lenenc_bytes(0);
  EXPECT_EQ(res.size(), length);
  EXPECT_EQ(res[length - 1], filler);
}


