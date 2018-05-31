/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "gmock/gmock.h"

#include "mysqlrouter/http_common.h"

#ifdef _WIN32
#include <WinSock2.h>
#endif

class HttpTimeTest : public ::testing::Test {
protected:
  virtual void SetUp() {
  }
};

TEST_F(HttpTimeTest, Something) {
  EXPECT_THAT(time_from_rfc5322_fixdate("Thu, 31 May 2018 15:18:20 GMT"), ::testing::Eq(1527779900));

  char date_buf[30];
  EXPECT_THAT(time_to_rfc5322_fixdate(1527779900, date_buf, sizeof(date_buf)), ::testing::Eq(29));
  EXPECT_THAT(date_buf, ::testing::StrEq("Thu, 31 May 2018 15:18:20 GMT"));
}

int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
