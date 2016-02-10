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

#include "gtest_consoleoutput.h"
#include "cmd_exec.h"
#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/datatypes.h"

#include <fstream>
#include <signal.h>
#include <string>
#include <thread>

using std::string;

string g_cwd;
Path g_origin;

class Bug22020711 : public ConsoleOutputTest {
public:
  void start_router() {
    string cmd = "ROUTER_PID=" + pid_path->str() + " " + app_mysqlrouter->str() + " -c " + config_path->str();
    auto cmd_result = cmd_exec(cmd, true);
  }

protected:
  virtual void SetUp() {
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(g_cwd));
    config_path->append("Bug22020711.ini");

    pid_path.reset(new Path(g_cwd));
    pid_path->append("test_pid");
    std::remove(pid_path->c_str());
  }

  void reset_config() {
    std::ofstream ofs_config(config_path->str());
    if (ofs_config.good()) {
      ofs_config << "[DEFAULT]\n";
      ofs_config << "logging_folder =\n";
      ofs_config << "plugin_folder = " << plugin_dir->str() << "\n";
      ofs_config << "runtime_folder = " << stage_dir->str() << "\n";
      ofs_config << "config_folder = " << stage_dir->str() << "\n\n";
      ofs_config.close();
    }
  }

  void recieve_message(int sockfd, mysql_protocol::Packet::vector_t &buffer) {
    auto buffer_length = buffer.size();
    ssize_t res = 0;
    size_t bytes_read = 0;

    res = read(sockfd, &buffer.front(), buffer_length);
    bytes_read += static_cast<size_t>(res);

    while (bytes_read < 4) {
      res = read(sockfd, &buffer.back(), buffer_length - bytes_read);
      bytes_read += static_cast<size_t>(res);
    }

    assert(buffer.size() >= 4);
    auto pkt_size = mysql_protocol::Packet(buffer).get_payload_size() + 4;

    while (bytes_read < pkt_size) {
      res = read(sockfd, &buffer.back(), buffer_length - bytes_read);
      bytes_read += static_cast<size_t>(res);
    }
  }

  pid_t get_router_pid() {
    std::ifstream pid_file(pid_path->str(), std::ifstream::in);
    if (pid_file.good()) {
      pid_t process_id = 0;
      assert(pid_file >> process_id);
      pid_file.close();
      return process_id;
    }
    return -1;
  }

  std::unique_ptr<Path> config_path;
  std::unique_ptr<Path> pid_path;
};

TEST_F(Bug22020711, NoValidDestinations) {
  reset_config();

  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing:c]\n";
  c  << "bind_address = 127.0.0.1:7004\n";
  c  << "destinations = localhost:13005\n";
  c  << "mode = read-only\n\n";
  c.close();

  std::thread router_thread(&Bug22020711::start_router, this);
  std::this_thread::sleep_for(std::chrono::seconds(1));

  mysqlrouter::TCPAddress addr("127.0.0.1", 7004);
  int router = routing::get_mysql_socket(addr, 2);
  ASSERT_GE(router, 0);

  auto fake_request = mysql_protocol::HandshakeResponsePacket(1, {}, "ROUTER", "", "fake_router_login");
  write(router, fake_request.data(), fake_request.size());

  mysql_protocol::Packet::vector_t buffer(14884984);
  recieve_message(router, buffer);

  EXPECT_NO_THROW({
    mysql_protocol::ErrorPacket packet = mysql_protocol::ErrorPacket(buffer);
    EXPECT_EQ(packet.get_message(), "Can't connect to MySQL server on '127.0.0.1'");
    EXPECT_EQ(packet.get_code(), 2003);
  });

  pid_t pid = get_router_pid();
  ASSERT_EQ(kill(pid, 15), 0);
  std::remove(pid_path->c_str());

  router_thread.join();
}

int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}