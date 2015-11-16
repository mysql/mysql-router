/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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
 * BUG22074209 --HELP OUTPUT DOES NOT DISPLAY VERSION
 *
 */

#include "gtest_consoleoutput.h"
#include "cmd_exec.h"
#include "router_app.h"

#include "gmock/gmock.h"

using ::testing::StartsWith;

class Bug22074209 : public ConsoleOutputTest {
protected:
  virtual void SetUp() {
    ConsoleOutputTest::SetUp();
  }
};

TEST_F(Bug22074209, HelpShowsVersion) {
  MySQLRouter r;
  std::string cmd = app_mysqlrouter->str() + " --help";

  auto cmd_result = cmd_exec(cmd, false);
  EXPECT_THAT(cmd_result.output, StartsWith(r.get_version_line()));
}
