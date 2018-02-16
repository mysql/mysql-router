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

  payload_size_ = read_int<uint32_t>(0, 3);

  if (!allow_partial && this->size() < payload_size_ + 4) {
    throw packet_error("Incorrect payload size (was " +
                       std::to_string(this->size()) + "; should be at least " + std::to_string(payload_size_) + ")");
  }

  sequence_id_ = (*this)[3];
}

void Packet::update_packet_size() {
  if (size() < 4)
    throw std::range_error("buffer not big enough");
  if (size() - 4 > kMaxAllowedSize)
    throw std::runtime_error("illegal packet size");

  // Update the size
  write_int<uint32_t>(*this, 0, static_cast<uint32_t>(size()) - 4, 3);
}

uint64_t Packet::read_adv_lenenc_uint(size_t& position) const {
  auto pr = read_lenenc_uint(position);  // throws range_error/runtime_error
  position += pr.second;
  return pr.first;
}

std::vector<uint8_t> Packet::read_adv_bytes(size_t& position, size_t length) const {
  std::vector<uint8_t> res = read_bytes(position, length); // throws range_error/runtime_error
  position += length;
  return res;
}

std::vector<uint8_t> Packet::read_adv_lenenc_bytes(size_t& position) const {
  auto pr = read_lenenc_bytes(position);  // throws range_error/runtime_error
  std::vector<uint8_t> res = pr.first;
  position += pr.second;
  return res;
}

std::string Packet::read_adv_string_nul(size_t& position) const {
  std::string res = read_string_nul(position);  // throws range_error/runtime_error
  position += res.size() + 1; // +1 for zero-terminator
  return res;
}

std::vector<uint8_t> Packet::read_adv_bytes_eof(size_t& position) const {
  std::vector<uint8_t> res = read_bytes_eof(position);  // throws range_error/runtime_error
  position += res.size();
  return res;
}

std::pair<uint64_t, size_t> Packet::read_lenenc_uint(size_t position) const {

  if (position >= size())
    throw std::range_error("start beyond EOF");
  if ((*this)[position] == 0xff ||  // 0xff is undefined in length encoded integers
      (*this)[position] == 0xfb)    // 0xfb represents NULL and not used in length encoded integers
    throw std::runtime_error("illegal value at first byte");

  // single-byte uint
  if ((*this)[position] < 0xfb) {
    return std::make_pair((*this)[position], 1);
  }

  // multi-byte uint
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
  if (position + length >= size())
    throw std::range_error("end beyond EOF");

  return std::make_pair(read_int<uint64_t>(position + 1, length), length + 1);
}

std::string Packet::read_string(unsigned long position, unsigned long length) const {

  if (static_cast<size_t>(position) > size()) {
    return "";
  }

  auto start = begin() + static_cast<vector_t::difference_type>(position);
  auto finish = (length == UINT_MAX) ? size() : position + length;
  auto it = std::find(start, begin() + static_cast<vector_t::difference_type>(finish), 0);
  return std::string(start, it);
}

std::string Packet::read_string_nul(size_t position) const {
  if (position >= size())
    throw std::range_error("start beyond EOF");

  auto it = std::find(begin() + position, end(), 0);
  if (it == end())
    throw std::runtime_error("zero-terminator not found");

  return std::string(begin() + position, it);
}

std::vector<uint8_t> Packet::read_bytes(size_t position, size_t length) const {

  if (position + length > size())
    throw std::range_error("start or end beyond EOF");

  return std::vector<uint8_t>(begin() + position, begin() + position + length);
}

std::pair<std::vector<uint8_t>, size_t> Packet::read_lenenc_bytes(size_t position) const {
  auto pr = read_lenenc_uint(position); // throws runtime_error, range_error

  size_t lenenc_uint_value = pr.first;
  size_t lenenc_uint_token_len = pr.second;

  size_t start = position + lenenc_uint_token_len;
  size_t endd = start + lenenc_uint_value;  // 'end' already exists
  if (endd > size())
    throw std::range_error("start or end beyond EOF");

  return make_pair(std::vector<uint8_t>(begin() + start, begin() + endd),
                   lenenc_uint_token_len + lenenc_uint_value);
}

std::vector<uint8_t> Packet::read_bytes_eof(size_t position) const {
  if (position >= size())
    throw std::range_error("start beyond EOF");

  return std::vector<uint8_t>(begin() + position, end());
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
