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

// must be the first header, don't move it
#include <gtest/gtest_prod.h>

// ignore GMock warnings
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif
#include "gmock/gmock.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <mysql.h>
#include "mysqlrouter/mysql_session.h"

class MySQLSessionTest : public ::testing::Test {};

using mysqlrouter::MySQLSession;

TEST_F(MySQLSessionTest, pasrse_ssl_mode) {
  EXPECT_EQ(SSL_MODE_DISABLED,        MySQLSession::parse_ssl_mode(MySQLSession::kSslModeDisabled));
  EXPECT_EQ(SSL_MODE_PREFERRED,       MySQLSession::parse_ssl_mode(MySQLSession::kSslModePreferred));
  EXPECT_EQ(SSL_MODE_REQUIRED,        MySQLSession::parse_ssl_mode(MySQLSession::kSslModeRequired));
  EXPECT_EQ(SSL_MODE_VERIFY_CA,       MySQLSession::parse_ssl_mode(MySQLSession::kSslModeVerifyCa));
  EXPECT_EQ(SSL_MODE_VERIFY_IDENTITY, MySQLSession::parse_ssl_mode(MySQLSession::kSslModeVerifyIdentity));
  EXPECT_THROW(MySQLSession::parse_ssl_mode("bad"),   std::logic_error);
}

TEST_F(MySQLSessionTest, ssl_mode_to_string) {
  EXPECT_EQ(MySQLSession::kSslModeDisabled,       MySQLSession::ssl_mode_to_string(SSL_MODE_DISABLED));
  EXPECT_EQ(MySQLSession::kSslModePreferred,      MySQLSession::ssl_mode_to_string(SSL_MODE_PREFERRED));
  EXPECT_EQ(MySQLSession::kSslModeRequired,       MySQLSession::ssl_mode_to_string(SSL_MODE_REQUIRED));
  EXPECT_EQ(MySQLSession::kSslModeVerifyCa,       MySQLSession::ssl_mode_to_string(SSL_MODE_VERIFY_CA));
  EXPECT_EQ(MySQLSession::kSslModeVerifyIdentity, MySQLSession::ssl_mode_to_string(SSL_MODE_VERIFY_IDENTITY));
}

