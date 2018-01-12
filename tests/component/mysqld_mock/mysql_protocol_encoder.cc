/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql_protocol_encoder.h"

#include <cassert>
#include <iostream>

namespace server_mock {

MySQLProtocolEncoder::MsgBuffer
MySQLProtocolEncoder::encode_ok_message(uint8_t seq_no,
                               uint64_t affected_rows,
                               uint64_t last_insert_id,
                               uint16_t status,
                               uint16_t warnings) {
  MsgBuffer out_buffer;

  encode_msg_begin(out_buffer);

  append_byte(out_buffer, 0x0);
  append_lenenc_int(out_buffer, affected_rows);
  append_lenenc_int(out_buffer, last_insert_id);
  append_int(out_buffer, status);
  append_int(out_buffer, warnings);

  encode_msg_end(out_buffer, seq_no);

  return out_buffer;
}

MySQLProtocolEncoder::MsgBuffer
MySQLProtocolEncoder::encode_error_message(uint8_t seq_no,
                                  uint16_t error_code,
                                  const std::string &sql_state,
                                  const std::string &error_msg) {
  MsgBuffer out_buffer;

  encode_msg_begin(out_buffer);

  append_byte(out_buffer, 0xff);
  append_int(out_buffer, error_code);
  append_byte(out_buffer, 0x23); // "#"
  append_str(out_buffer, sql_state);
  append_str(out_buffer, error_msg);

  encode_msg_end(out_buffer, seq_no);

  return out_buffer;
}

MySQLProtocolEncoder::MsgBuffer
MySQLProtocolEncoder::encode_greetings_message(uint8_t seq_no,
                                              const std::string &mysql_version,
                                              uint32_t connection_id,
                                              const std::string &nonce,
                                              mysql_protocol::Capabilities::Flags capabilities /*=...*/,
                                              uint8_t character_set,
                                              uint16_t status_flags) {
  MsgBuffer out_buffer;

  encode_msg_begin(out_buffer);

  append_byte(out_buffer, 0x0a);
  append_str(out_buffer, mysql_version);
  append_byte(out_buffer, 0x0); // NULL
  append_int(out_buffer, connection_id);
  append_str(out_buffer, nonce.substr(0,8));
  append_byte(out_buffer, 0x0); // filler
  append_int(out_buffer, capabilities.low_16_bits()); // cap_1
  append_byte(out_buffer, character_set);
  append_int(out_buffer, status_flags);
  append_int(out_buffer, capabilities.high_16_bits()); // cap_2
  append_byte(out_buffer, 0x0); // auth-plugin-len = 0
  append_str(out_buffer, std::string(10, '\0')); // reserved
  append_str(out_buffer, nonce.substr(8));
  append_byte(out_buffer, 0x0); // trialing \0

  encode_msg_end(out_buffer, seq_no);

  return out_buffer;
}

MySQLProtocolEncoder::MsgBuffer
MySQLProtocolEncoder::encode_columns_number_message(uint8_t seq_no, uint64_t number) {
  MsgBuffer out_buffer;
  encode_msg_begin(out_buffer);

  append_lenenc_int(out_buffer, number);

  encode_msg_end(out_buffer, seq_no);
  return out_buffer;
}

MySQLProtocolEncoder::MsgBuffer
MySQLProtocolEncoder::encode_column_meta_message(uint8_t seq_no,
                                                const column_info_type &column_info) {
  MsgBuffer out_buffer;
  encode_msg_begin(out_buffer);

  append_lenenc_str(out_buffer, column_info.catalog);
  append_lenenc_str(out_buffer, column_info.schema);
  append_lenenc_str(out_buffer, column_info.table);
  append_lenenc_str(out_buffer, column_info.orig_table);
  append_lenenc_str(out_buffer, column_info.name);
  append_lenenc_str(out_buffer, column_info.orig_name);

  MsgBuffer meta_buffer;
  append_int(meta_buffer, column_info.character_set);
  append_int(meta_buffer, column_info.length);
  append_byte(meta_buffer, static_cast<uint8_t>(column_info.type));
  append_int(meta_buffer, column_info.flags);
  append_byte(meta_buffer, column_info.decimals);
  append_int(meta_buffer, static_cast<uint16_t>(0));

  append_lenenc_int(out_buffer, meta_buffer.size());
  append_buffer(out_buffer, meta_buffer);

  encode_msg_end(out_buffer, seq_no);
  return out_buffer;
}

MySQLProtocolEncoder::MsgBuffer
MySQLProtocolEncoder::encode_row_message(uint8_t seq_no,
                                         const std::vector<column_info_type> &columns_info,
                                         const RowValueType &row_values) {
  MsgBuffer out_buffer;
  encode_msg_begin(out_buffer);

  if (columns_info.size() != row_values.size()) {
    throw std::runtime_error(std::string("columns_info.size() != row_values.size() ")
              + std::to_string(columns_info.size())
              + std::string("!=") +  std::to_string(row_values.size()));
  }

  for (size_t i = 0; i < row_values.size(); ++i) {
    if (row_values[i].first) {
      append_lenenc_str(out_buffer, row_values[i].second);
    } else {
      append_byte(out_buffer, 0xfb); // NULL
    }
  }

  encode_msg_end(out_buffer, seq_no);
  return out_buffer;
}

MySQLProtocolEncoder::MsgBuffer
MySQLProtocolEncoder::encode_eof_message(uint8_t seq_no, uint16_t status,
                                         uint16_t warnings) {
  MsgBuffer out_buffer;
  encode_msg_begin(out_buffer);

  append_byte(out_buffer, 0xfe);  // ok
  append_int(out_buffer, status);
  append_int(out_buffer, warnings);

  encode_msg_end(out_buffer, seq_no);
  return out_buffer;
}

void MySQLProtocolEncoder::encode_msg_begin(MsgBuffer &out_buffer) {
  // reserve space for header
  append_int(out_buffer, static_cast<uint32_t>(0x0));
}

void MySQLProtocolEncoder::encode_msg_end(MsgBuffer &out_buffer, uint8_t seq_no) {
  assert(out_buffer.size() >= 4);
  // fill the header
  uint32_t msg_len = static_cast<uint32_t>(out_buffer.size()) - 4;
  if (msg_len > 0xffffff) {
    throw std::runtime_error("Invalid message length: " + std::to_string(msg_len));
  }
  uint32_t header = msg_len | static_cast<uint32_t>(seq_no << 24);

  auto len = sizeof(header);
  for (size_t i = 0; len > 0; ++i, --len) {
    out_buffer[i] = static_cast<byte>(header);
    header = static_cast<decltype(header)>(header >> 8);
  }
}

void MySQLProtocolEncoder::append_byte(MsgBuffer& buffer, byte value) {
  buffer.push_back(value);
}

void MySQLProtocolEncoder::append_str(MsgBuffer &buffer, const std::string &value) {
  buffer.insert(buffer.end(), value.begin(), value.end());
}

void MySQLProtocolEncoder::append_buffer(MsgBuffer &buffer, const MsgBuffer &value) {
  buffer.insert(buffer.end(), value.begin(), value.end());
}

void MySQLProtocolEncoder::append_lenenc_int(MsgBuffer &buffer, uint64_t val) {
  if (val < 251) {
    append_byte(buffer, static_cast<byte>(val));
  }
  else if (val < (1 << 16)) {
    append_byte(buffer, 0xfc);
    append_int(buffer, static_cast<uint16_t>(val));
  }
  else {
    append_byte(buffer, 0xfe);
    append_int(buffer, val);
  }
}

void MySQLProtocolEncoder::append_lenenc_str(MsgBuffer &buffer, const std::string &value) {
  append_lenenc_int(buffer, value.length());
  append_str(buffer, value);
}

MySQLColumnType column_type_from_string(const std::string& type) {
  int res = 0;

  try {
    res =  std::stoi(type);
  }
  catch (const std::invalid_argument&) {
    if (type == "DECIMAL") return MySQLColumnType::DECIMAL;
    if (type == "TINY") return MySQLColumnType::TINY;
    if (type == "SHORT") return MySQLColumnType::SHORT;
    if (type == "LONG") return MySQLColumnType::LONG;
    if (type == "INT24") return MySQLColumnType::INT24;
    if (type == "LONGLONG") return MySQLColumnType::LONGLONG;
    if (type == "DECIMAL") return MySQLColumnType::DECIMAL;
    if (type == "NEWDECIMAL") return MySQLColumnType::NEWDECIMAL;
    if (type == "FLOAT") return MySQLColumnType::FLOAT;
    if (type == "DOUBLE") return MySQLColumnType::DOUBLE;
    if (type == "BIT") return MySQLColumnType::BIT;
    if (type == "TIMESTAMP") return MySQLColumnType::TIMESTAMP;
    if (type == "DATE") return MySQLColumnType::DATE;
    if (type == "TIME") return MySQLColumnType::TIME;
    if (type == "DATETIME") return MySQLColumnType::DATETIME;
    if (type == "YEAR") return MySQLColumnType::YEAR;
    if (type == "STRING") return MySQLColumnType::STRING;
    if (type == "VAR_STRING") return MySQLColumnType::VAR_STRING;
    if (type == "BLOB") return MySQLColumnType::BLOB;
    if (type == "SET") return MySQLColumnType::SET;
    if (type == "ENUM") return MySQLColumnType::ENUM;
    if (type == "GEOMETRY") return MySQLColumnType::GEOMETRY;
    if (type == "NULL") return MySQLColumnType::NULL_;
    if (type == "TINYBLOB") return MySQLColumnType::TINY_BLOB;
    if (type == "LONGBLOB") return MySQLColumnType::LONG_BLOB;
    if (type == "MEDIUMBLOB") return MySQLColumnType::MEDIUM_BLOB;

    throw std::invalid_argument("Unknown type: \"" + type + "\"");
  }

  return static_cast<MySQLColumnType>(res);
}

} // namespace
