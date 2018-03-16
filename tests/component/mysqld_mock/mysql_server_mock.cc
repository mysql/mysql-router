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

#include "mysql_server_mock.h"
#include "mysql_protocol_utils.h"

#include <cstring>
#include <functional>
#include <iostream>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <system_error>

#ifndef _WIN32
#  include <netdb.h>
#  include <netinet/in.h>
#  include <fcntl.h>
#  include <signal.h>
#  include <sys/un.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <netinet/tcp.h>
#  include <unistd.h>
#  include <regex.h>
#else
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
typedef long ssize_t;
#include <regex>
#endif

using namespace std::placeholders;

namespace {

bool pattern_matching(const std::string &s,
                      const std::string &pattern) {
#ifndef _WIN32
  regex_t regex;
  auto r = regcomp(&regex, pattern.c_str(), REG_EXTENDED);
  if (r) {
    throw std::runtime_error("Error compiling regex pattern: " + pattern);
  }
  r = regexec(&regex, s.c_str(), 0, NULL, 0);
  regfree(&regex);
  return (r == 0);
#else
  std::regex regex(pattern);
  return std::regex_match(s, regex);
#endif
}

} // unnamed namespace

namespace server_mock {

constexpr char kAuthCachingSha2Password[] = "caching_sha2_password";
constexpr char kAuthNativePassword[] = "mysql_native_password";
constexpr size_t kReadBufSize = 16 * 1024;  // size big enough to contain any packet we're likely to read

MySQLServerMock::MySQLServerMock(const std::string &expected_queries_file,
                                 unsigned bind_port, bool debug_mode):
  bind_port_(bind_port),
  debug_mode_(debug_mode),
  json_reader_(expected_queries_file),
  protocol_decoder_(&read_packet) {
  if (debug_mode_)
    std::cout << "\n\nExpected SQL queries come from file '"
              << expected_queries_file << "'\n\n" << std::flush;
}

MySQLServerMock::~MySQLServerMock() {
  if (listener_ > 0) {
#ifndef _WIN32
    ::shutdown(listener_, SHUT_RDWR);
#endif
    close_socket(listener_);
  }
}

void MySQLServerMock::run() {
  setup_service();
  handle_connections();
}

void MySQLServerMock::setup_service() {
  int err;
  struct addrinfo hints, *ainfo;

  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  err = getaddrinfo(nullptr, std::to_string(bind_port_).c_str(), &hints, &ainfo);
  if (err != 0) {
    throw std::runtime_error(std::string("getaddrinfo() failed: ") + gai_strerror(err));
  }

  std::shared_ptr<void> exit_guard(nullptr, [&](void*){freeaddrinfo(ainfo);});

  listener_ = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
  if (listener_ < 0) {
    throw std::runtime_error("socket() failed: " + get_socket_errno_str());
  }

  int option_value = 1;
  if (setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&option_value),
        static_cast<socklen_t>(sizeof(int))) == -1) {
    throw std::runtime_error("setsockopt() failed: " + get_socket_errno_str());
  }

  err = bind(listener_, ainfo->ai_addr, ainfo->ai_addrlen);
  if (err < 0) {
    throw std::runtime_error("bind() failed: " + get_socket_errno_str()
                             + "; port=" + std::to_string(bind_port_));
  }

  err = listen(listener_, kListenQueueSize);
  if (err < 0) {
    throw std::runtime_error("listen() failed: " + get_socket_errno_str());
  }
}

#ifndef _WIN32
static volatile std::atomic<sig_atomic_t> g_terminate { 0 };

// ensure that 'terminate'-flag is thread-safe and signal-safe
//
// * sigatomic_t is not thread-safe
// * std::atomic is not signal-safe by default (only lock-free std::atomics are signal-safe)
//
// assume that sig_atomic_t is int-or-long
// (in C++17 we could use std::atomic<sig_atomic_t>.is_always_lock_free)
static_assert((std::is_same<sig_atomic_t, int>::value && (ATOMIC_INT_LOCK_FREE == 2)) ||
    (std::is_same<sig_atomic_t, long>::value && (ATOMIC_LONG_LOCK_FREE == 2)), "expected sig_atomic_t to lock-free");

// Signal handler to catch SIGTERM.
static void sigterm_handler(int /* signo */) {
  g_terminate = 1;
}
#else
// as we don't use the signal handler on windows, we don't need a signal-safe type
static bool g_terminate = false;
#endif

void MySQLServerMock::send_handshake(
    socket_t client_socket,
    mysql_protocol::Capabilities::Flags our_capabilities) {

  constexpr const char* plugin_name = kAuthNativePassword;
  constexpr const char* plugin_data = "123456789|ABCDEFGHI|"; // 20 bytes

  std::vector<uint8_t> buf = protocol_encoder_.encode_greetings_message(
      0, "8.0.5", 1, plugin_data, our_capabilities, plugin_name);
  send_packet(client_socket, buf);
}

mysql_protocol::HandshakeResponsePacket MySQLServerMock::handle_handshake_response(
    socket_t client_socket,
    mysql_protocol::Capabilities::Flags our_capabilities) {

  typedef std::vector<uint8_t> MsgBuffer;
  using namespace mysql_protocol;

  uint8_t buf[kReadBufSize];
  size_t payload_size;
  constexpr size_t header_len = HandshakeResponsePacket::get_header_length();

  // receive handshake response packet
  {
    // reads all bytes or throws
    read_packet(client_socket, buf, header_len);

    if (HandshakeResponsePacket::read_sequence_id(buf) != 1)
      throw std::runtime_error("Handshake response packet with incorrect sequence number: " +
                               std::to_string(HandshakeResponsePacket::read_sequence_id(buf)));

    payload_size = HandshakeResponsePacket::read_payload_size(buf);
    assert(header_len + payload_size <= sizeof(buf));

    // reads all bytes or throws
    read_packet(client_socket, buf + header_len, payload_size);
  }

  // parse handshake response packet
  {
    HandshakeResponsePacket pkt(MsgBuffer(buf, buf + header_len + payload_size));
    try {
      pkt.parse_payload(our_capabilities);

      #if 0 // enable if you need to debug
      pkt.debug_dump();
      #endif
      return pkt;
    } catch (const std::runtime_error& e) {
      // Dump packet contents to stdout, so we can try to debug what went wrong.
      // Since parsing failed, this is also likely to throw. If it doesn't,
      // great, but we'll be happy to take whatever info the dump can give us
      // before throwing.
      try {
        pkt.debug_dump();
      } catch (...) {}

      throw;
    }
  }
}

void MySQLServerMock::handle_auth_switch(socket_t client_socket) {

  constexpr uint8_t seq_nr = 2;

  // send switch-auth request packet
  {
    constexpr const char* plugin_data = "123456789|ABCDEFGHI|";

    auto buf = protocol_encoder_.encode_auth_switch_message(
                   seq_nr, kAuthCachingSha2Password, plugin_data);
    send_packet(client_socket, buf);
  }

  // receive auth-data packet
  {
    using namespace mysql_protocol;
    constexpr size_t header_len = HandshakeResponsePacket::get_header_length();

    uint8_t buf[kReadBufSize];

    // reads all bytes or throws
    read_packet(client_socket, buf, header_len);

    if (HandshakeResponsePacket::read_sequence_id(buf) != seq_nr + 1)
      throw std::runtime_error("Auth-change response packet with incorrect sequence number: " +
                               std::to_string(HandshakeResponsePacket::read_sequence_id(buf)));

    size_t payload_size = HandshakeResponsePacket::read_payload_size(buf);
    assert(header_len + payload_size <= sizeof(buf));

    // reads all bytes or throws
    read_packet(client_socket, buf + header_len, payload_size);

    // for now, we ignore the contents we just read, because we always positively
    // authenticate the client
  }

}

void MySQLServerMock::send_fast_auth(socket_t client_socket) {
  // a mysql-8 client will send us a cache-256-password-scramble
  // and expects a \x03 back (fast-auth) + a OK packet
  // Here we send the 1st of the two.

  // pretend we do cached_sha256 fast-auth
  constexpr uint8_t seq_nr = 4;
  constexpr uint8_t fast_auth_cmd = 3;
  constexpr uint8_t payload_size_bytes[] = {1, 0, 0};
  constexpr uint8_t switch_auth[] = {payload_size_bytes[0], payload_size_bytes[1], payload_size_bytes[2],
                                     seq_nr, fast_auth_cmd};

  send_packet(client_socket, switch_auth, sizeof(switch_auth));
}


void MySQLServerMock::handle_connections() {
  struct sockaddr_storage client_addr;
  socklen_t addr_size = sizeof(client_addr);

  std::cout << "Starting to handle connections on port: " << bind_port_ << std::endl;

#ifndef _WIN32
  // Install the signal handler for SIGTERM.
  struct sigaction sig_action;
  sig_action.sa_handler = sigterm_handler;
  sigemptyset(&sig_action.sa_mask);
  sig_action.sa_flags = 0;
  sigaction(SIGTERM, &sig_action, NULL);
  sigaction(SIGINT, &sig_action, NULL);


  // start signal handling thread, new thread inherits the signal mask
  auto signal_handler = [&] {
    while (!g_terminate) {
      int ret = pause(); // sleep until the signal is delivered
      if (ret == -1) {
        std::cout << "Signal was caught: " << strerror(errno) << std::endl;
      }
    }
  };
  std::thread(signal_handler).detach();

  // all other threads should block SIGTERM and SIGINT
  struct sigaction thread_sig_action;
  sigemptyset(&thread_sig_action.sa_mask);
  thread_sig_action.sa_flags = 0;
  sigaddset(&thread_sig_action.sa_mask, SIGTERM);
  sigaddset(&thread_sig_action.sa_mask, SIGINT);
  pthread_sigmask(SIG_BLOCK, &thread_sig_action.sa_mask, NULL);
#endif

  uint64_t active_client_threads { 0 };

  std::condition_variable active_client_threads_cond;
  std::mutex active_client_threads_cond_m;

  auto connection_handler = [&](socket_t client_socket) -> void {
    // increment the active thread count
    {
      std::lock_guard<std::mutex> lk(active_client_threads_cond_m);
      active_client_threads++;
    }
    active_client_threads_cond.notify_all();

    // ... and make sure decrement it again on exit ... and close the socket
    std::shared_ptr<void> exit_guard(nullptr, [&](void*){
      close_socket(client_socket);

      {
        std::lock_guard<std::mutex> lk(active_client_threads_cond_m);
        active_client_threads--;
        active_client_threads_cond.notify_all();
      }
    });
    try {
      ////////////////////////////////////////////////////////////////////////////////
      //
      // This is the handshake packet that my server v8.0.5 emits:
      //
      //        <header >   v10  <--- server version
      //  0000: 6c00 0000   0a   38 2e30 2e35 2d65 6e74 6572 7072 6973 652d 636f 6d6d 6572 6369 616c            l....8.0.5-enterprise-commercial
      //
      //                         server version -> <conn id> <-- auth data 1 -->  zero cap.low char status
      //  0020: 2d61 6476 616e 6365 642d 6c6f 6700 0800 0000 5b09 4e78 3d48 0a11   00   ff ff   ff   0200       -advanced-log.....[.Nx=H........
      //
      //      cap.hi  auth-len  <- reserved 10 0-bytes ->   <SECURE_CONN && auth-data 2    >   <PLUGIN_AUTH && auth-plugin name
      //  0040: ffc3     15     00 0000 0000 0000 0000 00   64 1242 070c 5263 2d01 710c 4100   6361 6368 696e   .............d.B..Rc-.q.A.cachin
      //
      //        auth-plugin name --------------------->
      //  0060: 675f 7368 6132 5f70 6173 7377 6f72 6400                                                         g_sha2_password.
      //
      //
      //  client v8.0.5 reponds with capability flags: 05ae ff01
      //
      ////////////////////////////////////////////////////////////////////////////////

      using namespace mysql_protocol;

      constexpr Capabilities::Flags our_capabilities = Capabilities::PROTOCOL_41
                                                     | Capabilities::PLUGIN_AUTH
                                                     | Capabilities::SECURE_CONNECTION;

      send_handshake(client_socket, our_capabilities);
      HandshakeResponsePacket handshake_response = handle_handshake_response(
                                                       client_socket, our_capabilities);

      uint8_t packet_seq = 2u;
      if (handshake_response.get_auth_plugin() == kAuthCachingSha2Password) {
        // typically, client >= 8.0.4 will trigger this branch

        handle_auth_switch(client_socket);
        send_fast_auth(client_socket);
        packet_seq += 3;  // 2 from auth-switch + 1 from fast-auth

      } else if (handshake_response.get_auth_plugin() == kAuthNativePassword) {
        // typically, client <= 5.7 will trigger this branch; do nothing, we're good
      } else {
        // unexpected auth-plugin name
        assert(0);
      }

      send_ok(client_socket, packet_seq);

      bool res = process_statements(client_socket);
      if (!res) {
        std::cout << "Error processing statements with client: " << client_socket << std::endl;
      }
    }
    catch (const std::exception &e) {
      std::cerr << "Exception caught in connection loop: " <<  e.what() << std::endl;
    }
  };

  while (!g_terminate) {
    fd_set fds;
    FD_ZERO (&fds);
    FD_SET (listener_, &fds);

    // timeval is initialized in loop because value of timeval may be override by calling select.
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000;

    int err = select (listener_ + 1, &fds, NULL, NULL, &tv);

    if (err < 0) {
      std::cerr << "select() failed: " << strerror(errno) << "\n";
      break;
    } else if (err == 0) {
      // timeout
      continue;
    }

    if (FD_ISSET(listener_, &fds)) {
      socket_t client_socket = accept(listener_, (struct sockaddr*)&client_addr, &addr_size);
      if (client_socket < 0) {
        // if we got interrupted at shutdown, just leave
        if (g_terminate) break;

        throw std::runtime_error("accept() failed: " + get_socket_errno_str());
      }

      // if it doesn't work, no problem.
      int one = 1;
      setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&one), sizeof(one));

      std::cout << "Accepted client " << client_socket << std::endl;
      std::thread(connection_handler, client_socket).detach();
    }
  }

  // wait until all threads have shutdown
  std::unique_lock<std::mutex> lk(active_client_threads_cond_m);
  active_client_threads_cond.wait(lk, [&]{ return active_client_threads == 0;});
}

bool MySQLServerMock::process_statements(socket_t client_socket) {
  using mysql_protocol::Command;

  while (true) {
    protocol_decoder_.read_message(client_socket);
    auto cmd = protocol_decoder_.get_command_type();
    switch (cmd) {
    case Command::QUERY: {
      std::string statement_received = protocol_decoder_.get_statement();
      const auto &next_statement = json_reader_.get_next_statement();

      bool statement_matching{false};
      if (!next_statement.statement_is_regex) { // not regex
        statement_matching = (statement_received == next_statement.statement);
      } else { // regex
        statement_matching = pattern_matching(statement_received,
                                              next_statement.statement);
      }

      if (debug_mode_) {
        // debug trace: show SQL statement that was received vs what was expected
        std::cout << "vvvv---- received statement ----vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n"
          << statement_received << std::endl
          << "----\n"
          << next_statement.statement << std::endl
          << "^^^^---- expected statement ----^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n"
          << (statement_matching ? "[MATCH OK]\n" : "[MATCH FAILED]\n\n\n\n") << std::flush;
      }

      if (!statement_matching) {
        uint8_t packet_seq = protocol_decoder_.packet_seq() + 1; // rollover to 0 is ok
        std::this_thread::sleep_for(json_reader_.get_default_exec_time());
        send_error(client_socket, packet_seq, MYSQL_PARSE_ERROR,
            std::string("Unexpected stmt, got: \"") + statement_received +
            "\"; expected: \"" + next_statement.statement + "\"");
      } else {
        handle_statement(client_socket, protocol_decoder_.packet_seq(), next_statement);
      }
    }
    break;
    case Command::QUIT:
      std::cout << "received QUIT command from the client" << std::endl;
      return true;
    default:
      std::cerr << "received unsupported command from the client: "
                << static_cast<int>(cmd) << "\n";
      uint8_t packet_seq = protocol_decoder_.packet_seq() + 1;   // rollover to 0 is ok
      std::this_thread::sleep_for(json_reader_.get_default_exec_time());
      send_error(client_socket, packet_seq, 1064, "Unsupported command: " + std::to_string(cmd));
    }
  }

  return true;
}

static void debug_trace_result(const ResultsetResponse *resultset) {
  std::cout << "QUERY RESULT:\n";
  for (size_t i = 0; i < resultset->rows.size(); ++i) {
    for (const auto& cell : resultset->rows[i])
      std::cout << "  |  " << (cell.first ? cell.second : "NULL");
    std::cout << "  |\n";
  }
  std::cout << "\n\n\n" << std::flush;
}

void MySQLServerMock::handle_statement(socket_t client_socket, uint8_t seq_no,
                    const StatementAndResponse& statement) {
  using StatementResponseType = StatementAndResponse::StatementResponseType;

  switch (statement.response_type) {
  case StatementResponseType::STMT_RES_OK: {
    if (debug_mode_) std::cout << std::endl;  // visual separator
    OkResponse *response = dynamic_cast<OkResponse *>(statement.response.get());
    std::this_thread::sleep_for(statement.exec_time);
    send_ok(client_socket, static_cast<uint8_t>(seq_no+1), 0, response->last_insert_id, 0, response->warning_count);
  }
  break;
  case StatementResponseType::STMT_RES_RESULT: {
    ResultsetResponse *response = dynamic_cast<ResultsetResponse *>(statement.response.get());
    if (debug_mode_) {
      debug_trace_result(response);
    }
    seq_no = static_cast<uint8_t>(seq_no + 1);
    auto buf = protocol_encoder_.encode_columns_number_message(seq_no++, response->columns.size());
    std::this_thread::sleep_for(statement.exec_time);
    send_packet(client_socket, buf);
    for (const auto& column: response->columns) {
      auto col_buf = protocol_encoder_.encode_column_meta_message(seq_no++, column);
      send_packet(client_socket, col_buf);
    }
    buf = protocol_encoder_.encode_eof_message(seq_no++);
    send_packet(client_socket, buf);

    for (size_t i = 0; i < response->rows.size(); ++i) {
      auto res_buf = protocol_encoder_.encode_row_message(seq_no++, response->columns, response->rows[i]);
      send_packet(client_socket, res_buf);
    }
    buf = protocol_encoder_.encode_eof_message(seq_no++);
    send_packet(client_socket, buf);
  }
  break;
  case StatementResponseType::STMT_RES_ERROR: {
    if (debug_mode_) std::cout << std::endl;  // visual separator
    ErrorResponse *response = dynamic_cast<ErrorResponse *>(statement.response.get());
    send_error(client_socket, static_cast<uint8_t>(seq_no+1), response->code, response->msg);
  }
  break;
  default:;
    throw std::runtime_error("Unsupported command in handle_statement(): " +
      std::to_string((int)statement.response_type));
  }
}

void MySQLServerMock::send_error(socket_t client_socket, uint8_t seq_no,
                                 uint16_t error_code,
                                 const std::string &error_msg,
                                 const std::string &sql_state) {
  auto buf = protocol_encoder_.encode_error_message(seq_no, error_code,
                                                    sql_state, error_msg);
  send_packet(client_socket, buf);
}

void MySQLServerMock::send_ok(socket_t client_socket, uint8_t seq_no,
    uint64_t affected_rows,
    uint64_t last_insert_id,
    uint16_t server_status,
    uint16_t warning_count) {
  auto buf = protocol_encoder_.encode_ok_message(seq_no, affected_rows, last_insert_id, server_status, warning_count);
  send_packet(client_socket, buf);
}

} // namespace server_mock
