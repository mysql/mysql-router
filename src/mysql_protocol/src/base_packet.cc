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

#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/utils.h"

#include <algorithm>
#include <limits>

using std::string;

namespace mysql_protocol {

Packet::Packet(const vector_t &buffer, uint32_t capabilities, bool allow_partial)
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

uint64_t Packet::get_lenenc_uint(size_t position) const {
  assert(size() >= 1);
  assert(position < size());
  assert((*this)[position] != 0xff); // 0xff is undefined in length encoded integers
  assert((*this)[position] != 0xfb); // 0xfb represents NULL and not used in length encoded integers

  if ((*this)[position] < 0xfb) {
    return (*this)[position];
  }

  size_t length = 2;
  switch ((*this)[position]) {
    case 0xfc:
      length = 2;
      break;
    case 0xfd:
      length = 3;
      break;
    case 0xfe:
      length = 8;
  }
  assert(size() >= length + 1);
  assert(position + length < size());
  return get_int<uint64_t>(position + 1, length);
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

Packet::vector_t Packet::get_lenenc_bytes(size_t position) const {
  // Cast to long so we can use it on iterators
  auto length = static_cast<long>(get_lenenc_uint(position));
  auto start = static_cast<long>(position);

  // Where does the actual data start
  switch ((*this)[position]) {
    case 0xfc:
      start += 3;
      break;
    case 0xfd:
      start += 4;
      break;
    case 0xfe:
      start += 9;
      break;
    default:
      start += 1;
  }

  return std::vector<uint8_t>(begin() + start, begin() + start + length);
}

void Packet::add(const Packet::vector_t &value) {
  insert(end(), value.begin(), value.end());
}

void Packet::add(const std::string &value) {
  insert(end(), value.begin(), value.end());
}

} // namespace mysql_protocol

