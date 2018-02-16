/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

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

//TODO This is already defined in ../../../tests/helpers/router_test_helpers.h, but
//     we don't want to include that (different sub-project).  Instead, it should be
//     moved to mysql_harness/shared/include/test/helpers.h first, and then #included
//     from here.
#define EXPECT_THROW_LIKE(expr, exc, msg) try { \
      expr;\
      ADD_FAILURE() << "Expected exception of type " #exc << " but got none\n";\
    } catch (exc &e) {\
      if (std::string(e.what()).find(msg) == std::string::npos) {\
          ADD_FAILURE() << "Expected exception with message: " << msg << "\nbut got: " << e.what() << "\n";\
      }\
    } catch (...) {\
      ADD_FAILURE() << "Expected exception of type " #exc << " but got another\n";\
    }

using std::string;
using ::testing::ContainerEq;
using ::testing::NotNull;

using mysql_protocol::Packet;
using namespace mysql_protocol;

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
    EXPECT_EQ(0U, p.get_capabilities().bits());
    EXPECT_EQ(0UL, p.get_payload_size());
  }

  {
    auto p = Packet(2);
    EXPECT_EQ(2, p.get_sequence_id());
    EXPECT_EQ(0U, p.get_capabilities().bits());
    EXPECT_EQ(0U, p.get_payload_size());
  }

  {
    auto p = Packet(2, Capabilities::PROTOCOL_41);
    EXPECT_EQ(2, p.get_sequence_id());
    EXPECT_EQ(Capabilities::PROTOCOL_41, p.get_capabilities());
    EXPECT_EQ(0U, p.get_payload_size());
  }
}

TEST_F(MySQLProtocolPacketTest, CopyConstructor) {
  {
    Packet p({0x1, 0x0, 0x0, 0x9, 0x32});
    Packet p_copy(p);
    ASSERT_EQ(p.size(), p_copy.size());
    ASSERT_EQ(p.get_sequence_id(), p_copy.get_sequence_id());
    ASSERT_EQ(0U, p_copy.get_capabilities().bits());
  }

  {
    Packet p({0x1, 0x0, 0x0, 0x9, 0x32}, Capabilities::PROTOCOL_41);
    Packet p_copy(p);
    ASSERT_EQ(p.size(), p_copy.size());
    ASSERT_EQ(p.get_sequence_id(), p_copy.get_sequence_id());
    ASSERT_EQ(Capabilities::PROTOCOL_41, p_copy.get_capabilities());
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
    ASSERT_EQ(0U, p_copy.get_capabilities().bits());
  }

  {
    Packet p({0x1, 0x0, 0x0, 0x9, 0x32}, Capabilities::PROTOCOL_41);
    Packet p_copy{};
    p_copy = p;
    ASSERT_EQ(p.size(), p_copy.size());
    ASSERT_EQ(p.get_sequence_id(), p_copy.get_sequence_id());
    ASSERT_EQ(p.get_payload_size(), p_copy.get_payload_size());
    ASSERT_EQ(p.get_capabilities(), p_copy.get_capabilities());
  }
}

TEST_F(MySQLProtocolPacketTest, MoveConstructor) {
  Packet::vector_t buffer = {0x1, 0x0, 0x0, 0x9, 0x32};
  {
    Packet p(buffer, Capabilities::PROTOCOL_41);
    Packet q(std::move(p));

    ASSERT_EQ(buffer.size(), q.size());
    ASSERT_EQ(Capabilities::PROTOCOL_41, q.get_capabilities());
    ASSERT_EQ(9U, q.get_sequence_id());
    ASSERT_EQ(1U, q.get_payload_size());

    // original should be empty and re-set
    ASSERT_EQ(0U, p.size());
    ASSERT_EQ(0U, p.get_capabilities().bits());
    ASSERT_EQ(0U, p.get_sequence_id());
    ASSERT_EQ(0U, p.get_payload_size());
  }
}

TEST_F(MySQLProtocolPacketTest, MoveAssigment) {
  Packet::vector_t buffer = {0x1, 0x0, 0x0, 0x9, 0x32};
  {
    Packet p(buffer, Capabilities::PROTOCOL_41);
    Packet q{};
    q = std::move(p);

    ASSERT_EQ(buffer.size(), q.size());
    ASSERT_EQ(Capabilities::PROTOCOL_41, q.get_capabilities());
    ASSERT_EQ(9U, q.get_sequence_id());
    ASSERT_EQ(1U, q.get_payload_size());

    // original should be empty and re-set
    ASSERT_EQ(0U, p.size());
    ASSERT_EQ(0U, p.get_capabilities().bits());
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

TEST_F(MySQLProtocolPacketTest, WriteInt_invalid_input) {

  Packet buf10({0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0}, true);
  Packet pkt(buf10, true);
  constexpr uint8_t foo = 42;

  // start beyond EOF
  {
    EXPECT_NO_THROW(Packet::write_int<uint8_t>(pkt, 9, foo));
  }
  {
    EXPECT_THROW_LIKE(
      Packet::write_int<uint8_t>(pkt, 10, foo),
      std::range_error,
      "start or end beyond EOF"
    );
  }

  // end beyond EOF
  {
    EXPECT_NO_THROW(Packet::write_int<uint32_t>(pkt, 6, foo));
  }
  {
    EXPECT_THROW_LIKE(
      Packet::write_int<uint32_t>(pkt, 7, foo),
      std::range_error,
      "start or end beyond EOF"
    );
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

    // Do not change the 0x0086 constant. Accidentally, it tests for
    // optimization-related bugs in some versions of GCC.
    p.add_int<uint16_t>(0x0086);
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

TEST_F(MySQLProtocolPacketTest, PackLenEncodedInt) {

  using V8 = std::vector<uint8_t>;

  // 1-byte
  {
    Packet buf;
    EXPECT_EQ(1, buf.add_lenenc_uint(0u));
    EXPECT_EQ(V8({0u}), buf);
  }
  {
    Packet buf;
    EXPECT_EQ(1, buf.add_lenenc_uint(250u));
    EXPECT_EQ(V8({250u}), buf);
  }

  // 3-byte
  {
    Packet buf;
    EXPECT_EQ(3, buf.add_lenenc_uint(251u));
    EXPECT_EQ(V8({0xfc, 251u, 0u}), buf);
  }
  {
    Packet buf;
    EXPECT_EQ(3, buf.add_lenenc_uint(0x1234));
    EXPECT_EQ(V8({0xfc, 0x34, 0x12}), buf);
  }
  {
    Packet buf;
    EXPECT_EQ(3, buf.add_lenenc_uint(0xffff));
    EXPECT_EQ(V8({0xfc, 0xff, 0xff}), buf);
  }

  // 4-byte
  {
    Packet buf;
    EXPECT_EQ(4, buf.add_lenenc_uint(0x010000));
    EXPECT_EQ(V8({0xfd, 0u, 0u, 1u}), buf);
  }
  {
    Packet buf;
    EXPECT_EQ(4, buf.add_lenenc_uint(0x123456));
    EXPECT_EQ(V8({0xfd, 0x56, 0x34, 0x12}), buf);
  }
  {
    Packet buf;
    EXPECT_EQ(4, buf.add_lenenc_uint(0xffffff));
    EXPECT_EQ(V8({0xfd, 0xff, 0xff, 0xff}), buf);
  }

  // 9-byte
  {
    Packet buf;
    EXPECT_EQ(9, buf.add_lenenc_uint(0x01000000));
    EXPECT_EQ(V8({0xfe,   0u, 0u, 0u, 1u,   0u, 0u, 0u, 0u}), buf);
  }
  {
    Packet buf;
    EXPECT_EQ(9, buf.add_lenenc_uint(0x1234567890abcdef));
    EXPECT_EQ(V8({0xfe,   0xef, 0xcd, 0xab, 0x90,   0x78, 0x56, 0x34, 0x12}), buf);
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
    p.add_int<int64_t>(-4294967295LL);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x01, 0x0, 0x0, 0x0, 0xff, 0xff, 0xff, 0xff}));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackUInt8) {
  {
    Packet buf{0x10};
    EXPECT_EQ(16, buf.read_int<uint8_t>(0));
  }
  {
    Packet buf{0x10, 0x20};
    EXPECT_EQ(32, buf.read_int<uint8_t>(1));
  }

  {
    Packet buf{0x10};
    EXPECT_EQ(16, buf.read_int<uint8_t>(0, 1));
  }
  {
    Packet buf{0x10, 0x20};
    EXPECT_EQ(16, buf.read_int<uint8_t>(0, 2));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackUInt16) {
  {
    Packet buf{0x10, 0x00};
    EXPECT_EQ(16, buf.read_int<uint16_t>(0, 2));
  }

  {
    Packet buf{0x10, 0x20};
    EXPECT_EQ(8208, buf.read_int<uint16_t>(0));
  }

  {
    Packet buf{0x10, 0x20, 0x30};
    EXPECT_EQ(8208, buf.read_int<uint16_t>(0, 2));
  }

  {
    Packet buf{0xab, 0xba};
    EXPECT_EQ(47787, buf.read_int<uint16_t>(0));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackInt3Bytes) {

  // unsigned
  {
    Packet buf{0x10, 0x00, 0x00};
    EXPECT_EQ(16U, buf.read_int<uint32_t>(0, 3));
  }

  {
    Packet buf{0x10, 0x20, 0x00};
    EXPECT_EQ(8208U, buf.read_int<uint32_t>(0, 3));
  }

  {
    Packet buf{0x10, 0x20, 0x30};
    EXPECT_EQ(3153936U, buf.read_int<uint32_t>(0, 3));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackInt4bytes) {

  // unsigned
  {
    Packet buf({0x10, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(16U, buf.read_int<uint32_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x00, 0x00}, true);
    EXPECT_EQ(8208U, buf.read_int<uint32_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x30, 0x40}, true);
    EXPECT_EQ(1076895760U, buf.read_int<uint32_t>(0, 4));
  }

  {
    Packet buf({0x10, 0x20, 0x30, 0x40, 0x50}, true);
    EXPECT_EQ(1076895760U, buf.read_int<uint32_t>(0, 4));
  }

  // signed
  {
    Packet buf({0xff, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(-1, buf.read_int<int>(0));
  }

  {
    Packet buf({0xf2, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(-14, buf.read_int<int>(0));
  }

  {
    Packet buf({0xff, 0xff, 0xff, 0xfe}, true);
    EXPECT_EQ(-16777217, buf.read_int<int>(0));
  }

  {
    Packet buf({0xff, 0xff, 0xff, 0x7f}, true);
    EXPECT_EQ(2147483647, buf.read_int<int>(0, 4));
  }

  {
    Packet buf({0x02, 0x00, 0x00, 0x80}, true);
    EXPECT_EQ(-2147483646, buf.read_int<int>(0, 4));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackUInt64) {
  {
    Packet buf({0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(16UL, buf.read_int<uint64_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(8208UL, buf.read_int<uint64_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x30, 0x40, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(1076895760UL, buf.read_int<uint64_t>(0, 8));
  }

  {
    Packet buf({0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(4294967295UL, buf.read_int<uint64_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00, 0x80, 0x90}, true);
    EXPECT_EQ(9223372381529055248UL, buf.read_int<uint64_t>(0));
  }

  {
    Packet buf({0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(18446744073709551615UL, buf.read_int<uint64_t>(0));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackInt_invalid_input) {

  Packet buf10({0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0}, true);

  // supported sizes
  {
    for (size_t i : {1, 2, 3, 4, 8})
      buf10.read_int<uint64_t>(0, i); // shouldn't die
  }

  // unsuppposed sizes
  {
    for (size_t i : {0, 5, 6, 7, 9})
      EXPECT_DEATH_IF_SUPPORTED(buf10.read_int<uint64_t>(0, i), "");
  }

  // start beyond EOF
  {
    Packet buf{};
    EXPECT_THROW_LIKE(
      buf.read_int<uint64_t>(0, 1),
      std::range_error,
      "start or end beyond EOF"
    );
  }
  {
    EXPECT_NO_THROW(buf10.read_int<uint64_t>(9, 1));
  }
  {
    EXPECT_THROW_LIKE(
      buf10.read_int<uint64_t>(10, 1),
      std::range_error,
      "start or end beyond EOF"
    );
  }

  // end beyond EOF
  {
    EXPECT_NO_THROW(buf10.read_int<uint64_t>(6, 4));
  }
  {
    EXPECT_THROW_LIKE(
      buf10.read_int<uint64_t>(7, 4),
      std::range_error,
      "start or end beyond EOF"
    );
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackLenEncodedInt) {

  {
    Packet buf(Packet::vector_t{0xfa}, true);
    EXPECT_EQ(250U, buf.read_lenenc_uint(0).first);
    EXPECT_EQ(1U, buf.read_lenenc_uint(0).second);
  }

  {
    Packet buf({0xfc, 0xfb, 0x00}, true);
    EXPECT_EQ(251U, buf.read_lenenc_uint(0).first);
    EXPECT_EQ(3U, buf.read_lenenc_uint(0).second);
  }

  {
    Packet buf({0xfc, 0xff, 0xff}, true);
    EXPECT_EQ(65535U, buf.read_lenenc_uint(0).first);
    EXPECT_EQ(3U, buf.read_lenenc_uint(0).second);
  }

  {
    Packet buf({0xfd, 0x00, 0x00, 0x01}, true);
    EXPECT_EQ(65536U, buf.read_lenenc_uint(0).first);
    EXPECT_EQ(4U, buf.read_lenenc_uint(0).second);
  }

  {
    Packet buf({0xfd, 0xff, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(16777215U, buf.read_lenenc_uint(0).first);
    EXPECT_EQ(4U, buf.read_lenenc_uint(0).second);
  }

  // this test has special significance: if we parsed according to protocol v3.20
  // (which we don't implement atm), this would have to return 5U instead of 9U
  {
    Packet buf({0xfe, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(16777216U, buf.read_lenenc_uint(0).first);
    EXPECT_EQ(9U, buf.read_lenenc_uint(0).second);
  }

  {
    Packet buf({0xfe, 0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00, 0x80, 0x90}, true);
    EXPECT_EQ(9223372381529055248UL, buf.read_lenenc_uint(0).first);
    EXPECT_EQ(9U, buf.read_lenenc_uint(0).second);
  }

  {
    Packet buf({0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(ULLONG_MAX, buf.read_lenenc_uint(0).first);
    EXPECT_EQ(9U, buf.read_lenenc_uint(0).second);
  }
}

TEST_F(MySQLProtocolPacketTest, read_lenenc_uint) {

  // ok
  {
    Packet buf({0xfe, 0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00, 0x80}, true);
    EXPECT_NO_THROW(buf.read_lenenc_uint(0));
  }

  // start beyond EOF
  {
    Packet buf({0xfe, 0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00, 0x80}, true);
    EXPECT_THROW_LIKE(
      buf.read_lenenc_uint(10),
      std::range_error,
      "start beyond EOF"
    );
  }

  // end beyond EOF
  {
    Packet buf({0xfe, 0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00}, true);
    EXPECT_THROW_LIKE(
      buf.read_lenenc_uint(0),
      std::range_error,
      "end beyond EOF"
    );
  }

  // illegal first byte
  {
    Packet buf({0xfb}, true);
    EXPECT_THROW_LIKE(
      buf.read_lenenc_uint(0),
      std::runtime_error,
      "illegal value at first byte"
    );
  }
  {
    Packet buf({0xff}, true);
    EXPECT_THROW_LIKE(
      buf.read_lenenc_uint(0),
      std::runtime_error,
      "illegal value at first byte"
    );
  }
}

TEST_F(MySQLProtocolPacketTest, read_adv_lenenc_uint) {

  Packet buf({0xfe, 0x10, 0x20, 0x30,   0x40, 0x50, 0x00, 0x00,   0x80, 0xfe}, true);
  size_t pos = 0;
  EXPECT_NO_THROW(buf.read_adv_lenenc_uint(pos));
  EXPECT_EQ(9, pos);

  EXPECT_THROW_LIKE(
    buf.read_adv_lenenc_uint(pos),
    std::range_error,
    "end beyond EOF"
  );
  EXPECT_EQ(9, pos);
}

TEST_F(MySQLProtocolPacketTest, UnpackString) {
  {
    Packet p({'h', 'a', 'm', 0x0, 's', 'p', 'a', 'm'}, true);
    auto res = p.read_string(0);
    EXPECT_EQ(string("ham"), res);
    res = p.read_string(res.size() + 1UL);
    EXPECT_EQ(string("spam"), res);
    res = p.read_string(0, p.size());
    EXPECT_EQ(string("ham"), res);
  }

  {
    Packet p{};
    EXPECT_EQ(string{}, p.read_string(0));
  }

  {
    Packet p({'h', 'a', 'm', 's', 'p', 'a', 'm'}, true);
    EXPECT_EQ(string("hamspam"), p.read_string(0));
  }

  {
    Packet p({'h', 'a', 'm'}, true);
    EXPECT_EQ(string(""), p.read_string(30));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackStringLengthFixed) {
  Packet p({'h', 'a', 'm', 's', 'p', 'a', 'm'}, true);

  {
    auto res = p.read_string(0, 3);
    EXPECT_EQ(res, string("ham"));
  }

  {
    auto res = p.read_string(0, 2);
    EXPECT_EQ(res, string("ha"));
  }

  {
    auto res = p.read_string(3, 4);
    EXPECT_EQ(res, string("spam"));
  }
}

TEST_F(MySQLProtocolPacketTest, read_string_nul) {
  Packet p({'s', 'o', 'm', 'e', 0x0, 'n', 'o', 'z', 'e', 'r', 'o'}, true);

  EXPECT_EQ("some", p.read_string_nul(0));
  EXPECT_EQ("ome", p.read_string_nul(1));
  EXPECT_EQ("", p.read_string_nul(4));
  EXPECT_THROW_LIKE(
    p.read_string_nul(5),
    std::runtime_error,
    "zero-terminator not found"
  );
  EXPECT_THROW_LIKE(
    p.read_string_nul(10),
    std::runtime_error,
    "zero-terminator not found"
  );
  EXPECT_THROW_LIKE(
    p.read_string_nul(11),
    std::range_error,
    "start beyond EOF"
  );
}

TEST_F(MySQLProtocolPacketTest, read_adv_string_nul) {
  Packet p({'s', 'o', 'm', 'e', 0x0, 's', 't', 'r', 'i', 'n', 'g', 0x0, 'n', 'o', 'z', 'e', 'r', 'o'}, true);
  size_t pos = 0;

  EXPECT_EQ("some", p.read_adv_string_nul(pos));
  EXPECT_EQ(5, pos);

  EXPECT_EQ("string", p.read_adv_string_nul(pos));
  EXPECT_EQ(12, pos);

  EXPECT_THROW_LIKE(
    p.read_adv_string_nul(pos),
    std::runtime_error,
    "zero-terminator not found"
  );
  EXPECT_EQ(12, pos);
}

TEST_F(MySQLProtocolPacketTest, read_bytes) {
  Packet p({0x1, 0x0, 0x0, 0x9, 0x32});
  using V = std::vector<uint8_t>;

  EXPECT_EQ(V{}, p.read_bytes(0,0));
  EXPECT_EQ(V{0x1}, p.read_bytes(0,1));

  {
    V exp = {0x1, 0x0, 0x0, 0x9}; // doesn't build inline
    EXPECT_EQ(exp, p.read_bytes(0,4));
  }
  {
    V exp = {0x0, 0x0, 0x9, 0x32};
    EXPECT_EQ(exp, p.read_bytes(1,4));
  }

  EXPECT_THROW_LIKE(
    p.read_bytes(2,4),
    std::range_error,
    "start or end beyond EOF"
  );

  EXPECT_EQ(V{}, p.read_bytes(5,0));
}

TEST_F(MySQLProtocolPacketTest, read_adv_bytes) {
  Packet p({1, 0, 0, 9, 32});
  using V = std::vector<uint8_t>;

  size_t pos = 0;

  V exp = {1, 0, 0}; // doesn't build inline
  EXPECT_EQ(exp, p.read_adv_bytes(pos, 3));
  EXPECT_EQ(3, pos);

  EXPECT_THROW_LIKE(
    p.read_adv_bytes(pos, 3),
    std::runtime_error,
    "start or end beyond EOF"
  );
  EXPECT_EQ(3, pos);
}

TEST_F(MySQLProtocolPacketTest, read_bytes_eof) {
  Packet p({0x0, 0x9, 0x32, 0x0}, true);
  using V = std::vector<uint8_t>;

  {
    V exp = {0x0, 0x9, 0x32, 0x0}; // doesn't build inline
    EXPECT_EQ(exp, p.read_bytes_eof(0));
  }
  {
    V exp = {0x0};
    EXPECT_EQ(exp, p.read_bytes_eof(3));
  }

  EXPECT_THROW_LIKE(
    p.read_bytes_eof(4),
    std::range_error,
    "start beyond EOF"
  );
}

TEST_F(MySQLProtocolPacketTest, read_adv_bytes_eof) {
  Packet p({0x0, 0x9, 0x32, 0x0}, true);
  using V = std::vector<uint8_t>;

  size_t pos = 0;
  V exp = {0x0, 0x9, 0x32, 0x0}; // doesn't build inline

  EXPECT_EQ(exp, p.read_adv_bytes_eof(pos));
  EXPECT_EQ(4, pos);

  EXPECT_THROW_LIKE(
    p.read_adv_bytes_eof(pos),
    std::range_error,
    "start beyond EOF"
  );
}

TEST_F(MySQLProtocolPacketTest, UnpackBytesLengthEncoded1Byte) {
  Packet p({0x07, 'h', 'a', 'm', 's', 'p', 'a', 'm', 'f', 'o', 'o'}, true);
  auto pr = p.read_lenenc_bytes(0);
  EXPECT_THAT(pr.first, ContainerEq(Packet::vector_t{'h', 'a', 'm', 's', 'p', 'a', 'm'}));
  EXPECT_EQ(8, pr.second);
}

TEST_F(MySQLProtocolPacketTest, UnpackStringLengthEncoded3Bytes) {
  size_t length = 316;
  unsigned char filler = 0x4d;

  std::vector<uint8_t> data(length + 3, filler);
  data[0] = 0xfc;
  data[1] = 0x3c;
  data[2] = 0x01;
  Packet p(data, true);

  auto pr = p.read_lenenc_bytes(0);
  EXPECT_EQ(pr.first.size(), length);
  EXPECT_EQ(pr.first[0], filler);
  EXPECT_EQ(pr.first[length - 1], filler);
  EXPECT_EQ(length + 3, pr.second);
}

TEST_F(MySQLProtocolPacketTest, UnpackStringLengthEncoded8Bytes) {
  size_t length = 16777216;
  unsigned char filler = 0x4d;

  std::vector<uint8_t> data(length + 9, filler);
  std::vector<uint8_t> enc_length{0xfe, 0x0, 0x0, 0x0, 0x01, 0x0, 0x0, 0x0, 0x0};
  std::copy(enc_length.begin(), enc_length.end(), data.begin());
  Packet p(data, true);

  auto pr = p.read_lenenc_bytes(0);
  EXPECT_EQ(pr.first.size(), length);
  EXPECT_EQ(pr.first[length - 1], filler);
  EXPECT_EQ(length + 9, pr.second);
}

TEST_F(MySQLProtocolPacketTest, UnpackStringLengthEncoded8BytesWithNulByte) {
  size_t length = 16777216;
  unsigned char filler = 0x4d;

  std::vector<uint8_t> data(length + 9, filler);
  std::vector<uint8_t> enc_length{0xfe, 0x0, 0x0, 0x0, 0x01, 0x0, 0x0, 0x0, 0x0};
  std::copy(enc_length.begin(), enc_length.end(), data.begin());
  data[length / 2] = 0x0;
  Packet p(data, true);

  auto pr = p.read_lenenc_bytes(0);
  EXPECT_EQ(pr.first.size(), length);
  EXPECT_EQ(pr.first[length - 1], filler);
  EXPECT_EQ(length + 9, pr.second);
}

TEST_F(MySQLProtocolPacketTest, read_lenenc_bytes) {
  // throw scenarios for length-encoded uint are tested by read_lenenc_uint() test,
  // so here we only need to test for the cases of payload being beyond EOF

  Packet buf({4, 0x10, 0x20, 0x30, 0x40}, true);

  EXPECT_NO_THROW(buf.read_lenenc_bytes(0));

  buf.pop_back();
  EXPECT_THROW_LIKE(
    buf.read_lenenc_bytes(0),
    std::range_error,
    "start or end beyond EOF"
  );
}

TEST_F(MySQLProtocolPacketTest, read_adv_lenenc_bytes) {

  Packet buf({4, 0x10, 0x20, 0x30,   0x40, 2, 0x11, 0x22,   0x99}, true);
  size_t pos = 0;
  EXPECT_NO_THROW(buf.read_adv_lenenc_bytes(pos));
  EXPECT_EQ(5, pos);
  EXPECT_NO_THROW(buf.read_adv_lenenc_bytes(pos));
  EXPECT_EQ(8, pos);

  EXPECT_THROW_LIKE(
    buf.read_adv_lenenc_bytes(pos),
    std::range_error,
    "end beyond EOF"
  );
  EXPECT_EQ(8, pos);
}


int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
