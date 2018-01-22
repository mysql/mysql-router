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

#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/utils.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace mysql_protocol {

Packet::Packet(const vector_t &buffer, Capabilities::Flags capabilities, bool allow_partial)
    : vector(buffer), sequence_id_(0),
      payload_size_(0), capability_flags_(capabilities) {
  parse_header(allow_partial);
}

Packet::Packet(std::initializer_list<uint8_t> ilist) : Packet(vector_t(ilist)) {
  parse_header();
}

void Packet::parse_header(bool allow_partial) {
  if (size() < 4) {
    // do nothing when there are not enough bytes
    return;
  }

  payload_size_ = get_int<uint32_t>(0, 3);

  if (!allow_partial && this->size() < payload_size_ + 4) {
    throw packet_error("Incorrect payload size (was " +
                       std::to_string(this->size()) + "; should be at least " + std::to_string(payload_size_) + ")");
  }

  sequence_id_ = (*this)[3];
}

void Packet::update_packet_size() {
  // Update the size
  assert(size() >= 4);
  assert(size() - 4 < ((1ULL << (CHAR_BIT * 3)) - 1));
  write_int<uint32_t>(*this, 0, static_cast<uint32_t>(size()) - 4, 3);
}

std::pair<uint64_t, size_t> Packet::get_lenenc_uint(size_t position) const {
  assert(size() >= 1);
  assert(position < size());
  assert((*this)[position] != 0xff); // 0xff is undefined in length encoded integers
  assert((*this)[position] != 0xfb); // 0xfb represents NULL and not used in length encoded integers

  if ((*this)[position] < 0xfb) {
    return std::make_pair((*this)[position], 1);
  }

  size_t length = 2;
  switch ((*this)[position]) {
    case 0xfc:
      length = 2;
      break;
    case 0xfd:
      length = 3;
      break;
    case 0xfe:  // NOTE: up to MySQL 3.22 0xfe was follwed by 4 bytes, not 8
      length = 8;
  }
  assert(size() >= length + 1);
  assert(position + length < size());

  return std::make_pair(get_int<uint64_t>(position + 1, length),
                        length + 1);
}

std::string Packet::get_string(unsigned long position, unsigned long length) const {

  if (static_cast<size_t>(position) > size()) {
    return "";
  }

  auto start = begin() + static_cast<vector_t::difference_type>(position);
  auto finish = (length == UINT_MAX) ? size() : position + length;
  auto it = std::find(start, begin() + static_cast<vector_t::difference_type>(finish), 0);
  return std::string(start, it);
}

std::vector<uint8_t> Packet::get_bytes(size_t position, size_t length) const {
  assert (position + length <= size());
  return std::vector<uint8_t>(begin() + position, begin() + position + length);
}

Packet::vector_t Packet::get_lenenc_bytes(size_t position) const {
  auto pr = get_lenenc_uint(position);
  uint64_t length = pr.first;
  size_t start = position + pr.second;

  // length is uint64_t, which means it could be ridiculously high, much higher
  // than std::vector can support. Also, since the relation between limits is:
  //   vector::max_size() <= max(vector::size_type) <= max(uint64_t)
  // below assertion also guards against uint64_t high bits being lost.
  assert(length + start < std::vector<uint8_t>::max_size());

  return std::vector<uint8_t>(begin() + start, begin() + start + length);
}

void Packet::add(const Packet::vector_t &value) {
  insert(end(), value.begin(), value.end());
}

void Packet::add(const std::string &value) {
  insert(end(), value.begin(), value.end());
}

size_t Packet::add_lenenc_uint(uint64_t value) {

  // Specification is here: https://dev.mysql.com/doc/internals/en/integer.html
  //
  // To convert a number value into a length-encoded integer:
  //
  //   If the value is < 251,             it is stored as a 1-byte integer.
  //   If the value is ≥ 251 and < 2^16,  it is stored as 0xfc + 2-byte integer.
  //   If the value is ≥ 2^16 and < 2^24, it is stored as 0xfd + 3-byte integer.
  //   If the value is ≥ 2^24 and < 2^64, it is stored as 0xfe + 8-byte integer.

  constexpr uint64_t k2p16 = 1 << 16;
  constexpr uint64_t k2p24 = 1 << 24;

  if (value < 251) {
    push_back(static_cast<uint8_t>(value));
    return 1;
  } else if (value < k2p16) {
    push_back(0xfc);
    add_int<uint16_t>(static_cast<uint16_t>(value));
    return 3;
  } else if (value < k2p24) {
    push_back(0xfd);
    add_int(value, 3);
    return 4;
  } else {
    push_back(0xfe);
    add_int<uint64_t>(value);
    return 9;
  }
}

} // namespace mysql_protocol
