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

/**
 * BUG22020711 Show meaningful error when no backends available
 *
 */

#ifndef _WIN32 // fails on Windows due to race condition, disabled until fixed

#include "cmd_exec.h"
#include "gtest_consoleoutput.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/routing.h"
#include "router_test_helpers.h"

#include <fstream>
#include <functional>
#include <memory>
#include <signal.h>
#include <string>
#include <thread>
#ifdef _WIN32
#include <WinSock2.h>
#endif

using std::string;

string g_cwd;
Path   g_origin;

class Bug22020711 : public ConsoleOutputTest {
 public:
  void start_router() {
    string env = "ROUTER_PID=" + pid_path_->str();
    string cmd = app_mysqlrouter->str() + " -c " + config_path_->str();
    CmdExecResult cmd_result = cmd_exec(cmd, true, "", env);
  }

 protected:
  void SetUp() override {
    ConsoleOutputTest::SetUp();
    config_path_.reset(new Path(g_cwd));
    config_path_->append("Bug22020711.ini");

    pid_path_.reset(new Path(g_cwd));
    pid_path_->append("test_pid");
    std::remove(pid_path_->c_str());
  }

  void reset_config() {
    std::ofstream ofs_config(config_path_->str());
    if (ofs_config.good()) {
      ofs_config << "[DEFAULT]\n";
      ofs_config << "logging_folder =\n";
      ofs_config << "plugin_folder = " << plugin_dir->str() << "\n";
      ofs_config << "runtime_folder = " << stage_dir->str() << "\n";
      ofs_config << "config_folder = " << stage_dir->str() << "\n\n";
      ofs_config.close();
    }
  }

#ifndef _WIN32
  pid_t get_router_pid() {
    std::ifstream pid_file(pid_path_->str(), std::ifstream::in);
    if (pid_file.good()) {
      pid_t process_id = 0;
      if(!(pid_file >> process_id)) // replacement for: ASSERT_NE(pid_file >> process_id, 0);
        ADD_FAILURE() << "pid check failed";
      pid_file.close();
      return process_id;
    }
    return -2;  // 0 and -1 are valid values, which can cause a very fun bug if unchecked and passed to kill()
  }

  void kill_router() {
    pid_t pid = get_router_pid();
    if (pid > 0)
    {
      ASSERT_EQ(kill(pid, 15), 0);
    }
    std::remove(pid_path_->c_str());
  }
#else
  void kill_router() {
    throw std::runtime_error("Not implemented yet");
  }
#endif

  std::unique_ptr<Path> config_path_;
  std::unique_ptr<Path> pid_path_;
};

void receive_message(int sockfd, mysql_protocol::Packet::vector_t &buffer) {
  try {
    size_t   buffer_length = buffer.size();
    size_t   bytes_read;
    uint64_t timeout = 100;

    // read payload size
    constexpr size_t SIZE_FIELD_LEN = 4u;
    bytes_read = read_bytes_with_timeout(sockfd, &buffer[0], SIZE_FIELD_LEN, timeout);  // throws std::runtime_error
    ASSERT_EQ(bytes_read, SIZE_FIELD_LEN);
    uint32_t pkt_size = mysql_protocol::Packet(buffer).get_payload_size();

    // read the payload
    ASSERT_LE(pkt_size, buffer_length - SIZE_FIELD_LEN);
    bytes_read = read_bytes_with_timeout(sockfd, &buffer[4], pkt_size, timeout);        // throws std::runtime_error
    ASSERT_EQ(bytes_read, pkt_size);
  }

  catch (const std::runtime_error& e) {
    FAIL() << e.what();
  }
}

TEST_F(Bug22020711, NoValidDestinations) {

  // write first part to config file
  reset_config();

  // append config file with more stuff
  std::ofstream c(config_path_->str(), std::fstream::app | std::fstream::out);
  c << "[routing:c]\n";
  c  << "bind_address = 127.0.0.1:7004\n";
  c  << "destinations = localhost:13005\n";
  c  << "mode = read-only\n\n";
  c.close();

  // start router
  std::thread router_thread = std::thread(&Bug22020711::start_router, this);
  std::shared_ptr<void> scope_exit_trigger(nullptr, [this, &router_thread](void*){ // idiom to run lambda at scope exit
    kill_router();
    router_thread.join();
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // open a socket to router
  mysqlrouter::TCPAddress addr("127.0.0.1", 7004);
  int router = routing::get_mysql_socket(addr, 2);
  ASSERT_GE(router, 0);

  // send fake request packet
  mysql_protocol::HandshakeResponsePacket fake_request(1, {}, "ROUTER", "", "fake_router_login");
  long res = write(router, fake_request.data(), fake_request.size());
  ASSERT_EQ(static_cast<size_t>(res), fake_request.size());

  // receive response
  mysql_protocol::Packet::vector_t buffer(64);
  receive_message(router, buffer);

  // check the response
  EXPECT_NO_THROW({
    mysql_protocol::ErrorPacket packet = mysql_protocol::ErrorPacket(buffer);
    EXPECT_EQ(packet.get_message(), "Can't connect to MySQL server on '127.0.0.1'");
    EXPECT_EQ(packet.get_code(), 2003);
  });

}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#else

int main(int, char*) {
  return 0;
}

#endif // #ifndef _WIN32

