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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mysqlrouter/log_filter.h"

namespace mysqlrouter {

class LogFilterTest: public testing::Test {
 public:
  LogFilter log_filter;
};

TEST_F(LogFilterTest, IsStatementNotChangedWhenNoPatternMatched) {
  const std::string statement =
      "CREATE USER router_xxxx WITH mysql_native_password AS 'password123'";
  ASSERT_THAT(log_filter.filter(statement), testing::Eq(statement));
}

TEST_F(LogFilterTest, IsEmptyPasswordHiddenWhenPatternMatched) {
  const std::string statement =
      "CREATE USER router_xxxx WITH mysql_native_password AS ''";
  const std::string pattern(
      "CREATE USER ([[:graph:]]+) WITH mysql_native_password AS ([[:graph:]]*)");
  log_filter.add_pattern(pattern, 2);
  const std::string expected_result(
      "CREATE USER router_xxxx WITH mysql_native_password AS ***");
  ASSERT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

TEST_F(LogFilterTest, IsSpecialCharacterPasswordHiddenWhenPatternMatched) {
  const std::string statement =
      "CREATE USER router_xxxx WITH mysql_native_password AS '%$_*@'";
  const std::string pattern(
      "CREATE USER ([[:graph:]]+) WITH mysql_native_password AS ([[:graph:]]*)");
  log_filter.add_pattern(pattern, 2);
  const std::string expected_result(
      "CREATE USER router_xxxx WITH mysql_native_password AS ***");
  ASSERT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

TEST_F(LogFilterTest, IsPasswordHiddenWhenPatternMatched) {
  const std::string statement =
      "CREATE USER router_xxxx WITH mysql_native_password AS 'password123'";
  const std::string pattern(
      "CREATE USER ([[:graph:]]+) WITH mysql_native_password AS ([[:graph:]]*)");
  log_filter.add_pattern(pattern, 2);
  const std::string expected_result(
      "CREATE USER router_xxxx WITH mysql_native_password AS ***");
  ASSERT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

TEST_F(LogFilterTest, IsMoreThenOneGroupHidden) {
  const std::string statement =
      "ALTER USER \'jeffrey\'@\'localhost\' IDENTIFIED WITH sha256_password BY \'new_password\' PASSWORD EXPIRE INTERVAL 180 DAY";
  const std::string pattern =
      "ALTER USER ([[:graph:]]+) IDENTIFIED WITH ([[:graph:]]*) BY ([[:graph:]]*) PASSWORD EXPIRE INTERVAL 180 DAY";
  const std::string expected_result =
      "ALTER USER \'jeffrey\'@\'localhost\' IDENTIFIED WITH *** BY *** PASSWORD EXPIRE INTERVAL 180 DAY";
  log_filter.add_pattern(pattern, std::vector<size_t> {2, 3});
  ASSERT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

} // end of mysqlrouter namespace
