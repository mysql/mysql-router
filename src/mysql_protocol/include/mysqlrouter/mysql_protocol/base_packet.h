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

#ifndef MYSQLROUTER_MYSQL_PROTOCOL_BASE_PACKET_INCLUDED
#define MYSQLROUTER_MYSQL_PROTOCOL_BASE_PACKET_INCLUDED

#include <algorithm>
#include <climits>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mysql_protocol {

/** @class Packet
 * @brief Interface to MySQL packets
 *
 * This class is the base class for all the types of MySQL packets
 * such as ErrorPacket and HandshakeResponsePacket.
 *
 */
class Packet : public std::vector<uint8_t> {
 public:
  using vector_t = std::vector<uint8_t>;

  /** @brief Header length of packets */
  static const unsigned int kHeaderSize{4};

  /** @brief Default of max_allowed_packet defined by the MySQL Server (2^30) */
  static const unsigned int kMaxAllowedSize{1073741824};

  /** @brief Constructor */
  Packet() : Packet(0, 0) { }

  /** @overload
   *
   * This constructor takes a buffer, stores the data, and tries to get
   * information out of the buffer.
   *
   * When buffer is 4 or bigger, the payload size and sequence ID of the packet
   * is read from the first 4 bytes (packet header).
   *
   * When allow_partial is false, the payload size is not enforced and buffer
   * can be smaller than payload size given in the header. Allow partial packets
   * can be useful when all you need is to parse the heade
   *
   * @param buffer Vector of uint8_t
   * @param allow_partial Whether to allow buffers which have incomplete payload
   */
  explicit Packet(const vector_t &buffer, bool allow_partial = false)
      : Packet(buffer, 0, allow_partial) { }

  /** @overload
   *
   * @param buffer Vector of uint8_t
   * @param capabilities Server or Client capability flags
   * @param allow_partial Whether to allow buffers which have incomplete payload
   */
  Packet(const vector_t &buffer, uint32_t capabilities, bool allow_partial = false);

  /** @overload
   *
   * @param sequence_id Sequence ID of MySQL packet
   */
  explicit Packet(uint8_t sequence_id) : Packet(sequence_id, 0) { }

  /** @overload
   *
   * @param sequence_id Sequence ID of MySQL packet
   * @param capabilities Server or Client capability flags
   */
  Packet(uint8_t sequence_id, uint32_t capabilities)
      : vector(), sequence_id_(sequence_id),
        payload_size_(0), capability_flags_(capabilities) { }

  /** @overload */
  Packet(std::initializer_list<uint8_t> ilist);

  /** @brief Destructor */
  virtual ~Packet() { }

  /** @brief Copy Constructor */
  Packet(const Packet&) = default;

  /** @brief Move Constructor */
  Packet(Packet &&other) : vector(std::move(other)), sequence_id_(other.get_sequence_id()),
                           payload_size_(other.get_payload_size()),
                           capability_flags_(other.get_capabilities()) {
    other.sequence_id_ = 0;
    other.capability_flags_ = 0;
    other.payload_size_ = 0;
  }

  /** @brief Copy Assignment */
  Packet &operator=(const Packet &) = default;

  /** @brief Move Assigment */
  Packet &operator=(Packet &&other) {
    swap(other);
    sequence_id_ = other.sequence_id_;
    payload_size_ = other.payload_size_;
    capability_flags_ = other.get_capabilities();
    other.sequence_id_ = 0;
    other.capability_flags_ = 0;
    other.payload_size_ = 0;
    return *this;
  }

  /** @brief Gets an integral from given packet
   *
   * Gets an integral form a buffer at the given position. The size of the
   * integral is deduced from the give type but can be overwritten using
   * the size parameter.
   *
   * Supported are integral of 1, 2, 3, 4, or 8 bytes. To retrieve an 24 bit
   * integral it is necessary to use a 32-bit integral type and
   * provided the size, for example:
   *
   *     auto id = Packet::get_int<uint32_t>(buffer, 0, 3);
   *
   * In MySQL packets, integrals are stored using little-endian format.
   *
   * @param position Position where to start reading
   * @return integer type
   */
  template<typename Type, typename = std::enable_if<std::is_integral<Type>::value>>
  Type get_int(size_t position, size_t length = sizeof(Type)) const {
    assert((length >= 1 && length <= 4) || length == 8);
    assert(size() >= length);
    assert(position + length <= size());

    if (length == 1) {
      return static_cast<Type>((*this)[position]);
    }

    uint64_t result = 0;
    auto it = begin() + static_cast<long>(position + length);
    while (length-- > 0) {
      result <<= 8;
      result |= *--it;
    }

    return static_cast<Type>(result);
  }

  /** @brief Gets a length encoded integer from given packet
   *
   * @param position Position where to start reading
   * @return uint64_t
   */
  uint64_t get_lenenc_uint(size_t position) const;

  /** @brief Gets a string from packet
   *
   * Gets a string from the given buffer at the given position. When size is
   * not given, we read until the end of the buffer.
   * When nil byte is found before we reach the requested size, the string will be
   * not be size long (if size is not 0).
   *
   * When pos is greater than the size of the buffer, an empty string is returned.
   *
   * @param position Position from which to start reading
   * @param length Length of the string to read (default 0)
   * @return std::string
   */
  std::string get_string(unsigned long position,
                         unsigned long length = UINT_MAX) const;

  /** @brief Gets bytes from packet using length encoded size
   *
   * Gets bytes from buffer using the length encoded size.
   *
   * @param position Position from which to start reading
   * @return std::vector<uint8_t>
   */
  std::vector<uint8_t> get_lenenc_bytes(size_t position) const;

  /** @brief Packs and writes an integral to the buffer
   *
   * Packs and writes an integral to the buffer starting from the given
   * position.
   *
   * @param packet Vector of bytes to which we add
   * @param position Position where to write the integral
   * @param value Integral to add to the packet
   * @param size Size of the integral (default: size of integral)
   *
   */
  template<class Type, typename = std::enable_if<std::is_integral<Type>::value>>
  static void write_int(Packet &packet, size_t position,
                        Type value, size_t size = sizeof(Type)) {
    assert(std::numeric_limits<Type>::min() <= value && value <= std::numeric_limits<Type>::max());
    assert(position + size <= packet.size());

    auto i = position;
    uint64_t val = value;
    while (size-- > 0) {
      packet[i] = static_cast<uint8_t>(val);
      val >>= CHAR_BIT;
      ++i;
    }
  }

  /** @brief Packs and adds an integral to the buffer
   *
   * Packs and adds an integral to the given buffer.
   *
   * @param value Integral to add to the packet
   * @param size Size of the integral (default: size of integral)
   *
   */
  template<class Type, typename = std::enable_if<std::is_integral<Type>::value>>
  void add_int(Type value, size_t length = sizeof(Type)) {
    assert(std::numeric_limits<Type>::min() <= value && value <= std::numeric_limits<Type>::max());

    auto val = value;
    while (length-- > 0) {
      push_back(static_cast<uint8_t>(val));
      val = static_cast<Type>(val >> CHAR_BIT);
    }
  }

  /** @brief Adds bytes to the given packet
   *
   * Adds the given bytes to the buffer.
   *
   * @param value Bytes to add to the packet
   *
   */
  void add(const std::vector<uint8_t> &value);

  /** @brief Adds a string to the given packet
   *
   * Adds the given string to the buffer.
   *
   * @param value String to add to the packet
   */
  void add(const std::string &value);

  /** @brief Gets the packet sequence ID
   *
   * @return uint8_t
   */
  uint8_t get_sequence_id() const noexcept {
    return sequence_id_;
  }

  /** @brief Sets the packet sequence ID
   *
   * @param id Sequence ID of the packet
   */
  void set_sequence_id(uint8_t id) noexcept {
    sequence_id_ = id;
  }

  /** @brief Gets server/client capabilities
   *
   * @return uint32_t
   */
  uint32_t get_capabilities() const noexcept {
    return capability_flags_;
  }

  /** @brief Gets the payload size
   *
   * Returns the payload size parsed retrieved from the packet header.
   *
   * @return uint32_t
   */
  uint32_t get_payload_size() const noexcept {
    return payload_size_;
  }

 protected:

  /** @brief Resets packet
   *
   * Resets the packet and sets the sequence id.
   */
  void reset() {
    this->assign({0x0, 0x0, 0x0, sequence_id_});
  }

  /** @brief Updates payload size in packet header
   *
   * Updates the size of the payload storing it in the first 3 bytes
   * of the packet. This method is called after preparing the packet.
   */
  void update_packet_size();

  /** @brief MySQL packet sequence ID */
  uint8_t sequence_id_;

  /** @brief Payload of the packet */
  std::vector<uint8_t> payload_;

  /** @brief Payload size */
  std::uint32_t payload_size_;

  /** @brief Capability flags */
  uint32_t capability_flags_;

 private:

  void parse_header(bool allow_partial = false);
};

} // namespace mysql_protocol

#endif // MYSQLROUTER_MYSQLV10_BASE_PACKET_INCLUDED
