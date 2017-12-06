/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql_server_mock.h"

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

int get_socket_errno() {
#ifndef _WIN32
  return errno;
#else
  return WSAGetLastError();
#endif
}

std::string get_socket_errno_str() {
  return std::to_string(get_socket_errno());
}

void send_packet(socket_t client_socket, const uint8_t *data, size_t size, int flags = 0) {
  ssize_t sent = 0;
  size_t buffer_offset = 0;
  while (buffer_offset < size) {
    if ((sent = send(client_socket, reinterpret_cast<const char*>(data) + buffer_offset,
                     size-buffer_offset, flags)) < 0) {
      throw std::system_error(get_socket_errno(), std::system_category(), "send() failed");
    }
    buffer_offset += static_cast<size_t>(sent);
  }
}

void send_packet(socket_t client_socket,
                 const server_mock::MySQLProtocolEncoder::msg_buffer &buffer,
                 int flags = 0) {
  send_packet(client_socket, buffer.data(), buffer.size(), flags);
}

void read_packet(socket_t client_socket, uint8_t *data, size_t size, int flags = 0) {
  ssize_t received = 0;
  size_t buffer_offset = 0;
  while (buffer_offset < size) {
    received = recv(client_socket, reinterpret_cast<char*>(data)+buffer_offset,
                    size-buffer_offset, flags);
    if (received < 0) {
      throw std::system_error(get_socket_errno(), std::system_category(), "recv() failed");
    } else if (received == 0) {
      // connection closed by client
      throw std::runtime_error("recv() failed: Connection Closed");
    }
    buffer_offset += static_cast<size_t>(received);
  }
}

int close_socket(socket_t sock) {
#ifndef _WIN32
  return close(sock);
#else
  return closesocket(sock);
#endif
}

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

}

namespace server_mock {

MySQLServerMock::MySQLServerMock(const std::string &expected_queries_file,
                                 unsigned bind_port, bool debug_mode):
  bind_port_(bind_port),
  debug_mode_(debug_mode),
  json_reader_(expected_queries_file),
  protocol_decoder_(&read_packet) {
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
static volatile sig_atomic_t g_terminate = 0;

// Signal handler to catch SIGTERM.
static void sigterm_handler(int /* signo */) {
  g_terminate = 1;
}
#else
static bool g_terminate = false;
#endif

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
#endif

  uint64_t active_client_threads { 0 };

  std::condition_variable active_client_threads_cond;
  std::mutex active_client_threads_cond_m;

  auto connection_handler = [&](socket_t client_sock) -> void {
    // increment the active thread count
    {
      std::lock_guard<std::mutex> lk(active_client_threads_cond_m);
      active_client_threads++;
    }
    active_client_threads_cond.notify_all();

    // ... and make sure decrement it again on exit ... and close the socket
    std::shared_ptr<void> exit_guard(nullptr, [&](void*){
      close_socket(client_sock);

      {
        std::lock_guard<std::mutex> lk(active_client_threads_cond_m);
        active_client_threads--;
        active_client_threads_cond.notify_all();
      }
    });
    try {
      auto buf = protocol_encoder_.encode_greetings_message(0);
      send_packet(client_sock, buf);

      auto packet = protocol_decoder_.read_message(client_sock);
      auto packet_seq =  static_cast<uint8_t>(packet.packet_seq + 1);
      send_ok(client_sock, packet_seq);
      bool res = process_statements(client_sock);
      if (!res) {
        std::cout << "Error processing statements with client: " << client_sock << std::endl;
      }
    }
    catch (const std::exception &e) {
      std::cerr << "Exception caught in connection loop: " <<  e.what() << std::endl;
    }
  };

  while (!g_terminate) {
//    fd_set fds;
//    FD_ZERO (&fds);
//    FD_SET (listener_, &fds);

//    int err = select (listener_ + 1, &fds, NULL, NULL, NULL);
//    if (err < 0) {
//      break;
//    }

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

  // wait until all threads have shutdown
  std::unique_lock<std::mutex> lk(active_client_threads_cond_m);
  active_client_threads_cond.wait(lk, [&]{ return active_client_threads == 0;});
}

bool MySQLServerMock::process_statements(socket_t client_socket) {

  while (true) {
    auto packet = protocol_decoder_.read_message(client_socket);
    auto cmd = protocol_decoder_.get_command_type(packet);
    switch (cmd) {
    case MySQLCommand::QUERY: {
      std::string statement_received = protocol_decoder_.get_statement(packet);
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
        auto packet_seq = static_cast<uint8_t>(packet.packet_seq + 1);
        std::this_thread::sleep_for(json_reader_.get_default_exec_time());
        send_error(client_socket, packet_seq, MYSQL_PARSE_ERROR,
            std::string("Unexpected stmt, got: \"") + statement_received +
            "\"; expected: \"" + next_statement.statement + "\"");
      } else {
        handle_statement(client_socket, packet.packet_seq, next_statement);
      }
    }
    break;
    case MySQLCommand::QUIT:
      std::cout << "received QUIT command from the client" << std::endl;
      return true;
    default:
      std::cerr << "received unsupported command from the client: "
                << static_cast<int>(cmd) << "\n";
      auto packet_seq = static_cast<uint8_t>(packet.packet_seq + 1);
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
  using statement_response_type = StatementAndResponse::statement_response_type;

  switch (statement.response_type) {
  case statement_response_type::STMT_RES_OK: {
    OkResponse *response = dynamic_cast<OkResponse *>(statement.response.get());
    std::this_thread::sleep_for(statement.exec_time);
    send_ok(client_socket, static_cast<uint8_t>(seq_no+1), 0, response->last_insert_id, 0, response->warning_count);
  }
  break;
  case statement_response_type::STMT_RES_RESULT: {
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
  case statement_response_type::STMT_RES_ERROR: {
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

} // namespace
