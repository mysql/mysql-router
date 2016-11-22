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
 * BUG24909259 ROUTER IS NOT ABLE TO CONNECT TO M/C AFTER BOOSTRAPPED WITH DIR & NAME OPTIONS
 *
 */

#include <gtest/gtest_prod.h>

#include "router_app.h"
#include "config_parser.h"
#include "router_test_helpers.h"
#include "mysqlrouter/utils.h"
#include "keyring/keyring_manager.h"
#include "keyring/keyring_memory.h"

#include <fstream>
#include <string>

#include "gmock/gmock.h"

#ifdef _WIN32
static std::string kTestKRFile = "tkeyfile";
static std::string kTestKRFile2 = "tkeyfile2";
static struct Initter {
  Initter() {
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir)
      tmpdir = ".";
    kTestKRFile = std::string(tmpdir).append("/").append(kTestKRFile);
    kTestKRFile2 = std::string(tmpdir).append("/").append(kTestKRFile2);
  }
} init;
#else
static std::string kTestKRFile = "/tmp/tkeyfile";
static std::string kTestKRFile2 = "/tmp/tkeyfile2";
#endif
static std::string kTestKey = "mykey";

static std::string my_prompt_password(const std::string &, int *num_password_prompts) {
  *num_password_prompts = *num_password_prompts + 1;
  return kTestKey;
}

using namespace std::placeholders;

static void create_keyfile(const std::string &path) {
  mysqlrouter::delete_file(path);
  mysqlrouter::delete_file(path+".master");
  mysql_harness::init_keyring(path, path+".master", true);
  mysql_harness::reset_keyring();
}

static void create_keyfile_withkey(const std::string &path, const std::string &key) {
  mysqlrouter::delete_file(path);
  mysql_harness::init_keyring_with_key(path, key, true);
  mysql_harness::reset_keyring();
}


//bug#24909259
TEST(Bug24909259, PasswordPrompt_plain) {
  create_keyfile(kTestKRFile);
  create_keyfile_withkey(kTestKRFile2, kTestKey);

  int num_password_prompts = 0;
  mysqlrouter::set_prompt_password(std::bind(my_prompt_password, _1, &num_password_prompts));

  // metadata_cache
  mysql_harness::reset_keyring();
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  {
    mysql_harness::Config config(mysql_harness::Config::allow_keys);
    std::stringstream ss("[metadata_cache]\n");
    config.read(ss);

    MySQLRouter router;
    router.init_keyring(config);
    EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
    EXPECT_EQ(0, num_password_prompts);
  }
  mysql_harness::reset_keyring();
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  {
    mysql_harness::Config config(mysql_harness::Config::allow_keys);
    std::stringstream ss("[metadata_cache]\nuser=foo\n");
    config.read(ss);

    MySQLRouter router;
    ASSERT_THROW(
      router.init_keyring(config),
      std::runtime_error);
    EXPECT_EQ(1, num_password_prompts);
    EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  }
  mysql_harness::reset_keyring();
  {
    mysql_harness::Config config(mysql_harness::Config::allow_keys);
    std::stringstream ss("[DEFAULT]\nkeyring_path="+kTestKRFile2+"\n[metadata_cache]\nuser=foo\n");
    config.read(ss);

    MySQLRouter router;
    router.init_keyring(config);
    EXPECT_EQ(2, num_password_prompts);
    EXPECT_TRUE(mysql_harness::get_keyring() != nullptr);
  }
  mysql_harness::reset_keyring();
  {
    // this one should succeed completely
    mysql_harness::Config config(mysql_harness::Config::allow_keys);
    std::stringstream ss("[DEFAULT]\nkeyring_path="+kTestKRFile+"\nmaster_key_path="+kTestKRFile+".master\n[metadata_cache]\nuser=foo\n");
    config.read(ss);

    MySQLRouter router;
    router.init_keyring(config);
    EXPECT_TRUE(mysql_harness::get_keyring() != nullptr);
    EXPECT_EQ(2, num_password_prompts);
  }
  mysql_harness::reset_keyring();
}

TEST(Bug24909259, PasswordPrompt_keyed) {
  create_keyfile(kTestKRFile);
  create_keyfile_withkey(kTestKRFile2, kTestKey);

  int num_password_prompts = 0;
  mysqlrouter::set_prompt_password(std::bind(my_prompt_password, _1, &num_password_prompts));

  // metadata_cache
  mysql_harness::reset_keyring();
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  {
    mysql_harness::Config config(mysql_harness::Config::allow_keys);
    std::stringstream ss("[metadata_cache:foo]\n");
    config.read(ss);

    MySQLRouter router;
    router.init_keyring(config);
    EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
    EXPECT_EQ(0, num_password_prompts);
  }
  mysql_harness::reset_keyring();
  EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  {
    mysql_harness::Config config(mysql_harness::Config::allow_keys);
    std::stringstream ss("[metadata_cache:foo]\nuser=foo\n");
    config.read(ss);

    MySQLRouter router;
    ASSERT_THROW(
      router.init_keyring(config),
      std::runtime_error);
    EXPECT_EQ(1, num_password_prompts);
    EXPECT_TRUE(mysql_harness::get_keyring() == nullptr);
  }
  mysql_harness::reset_keyring();
  {
    mysql_harness::Config config(mysql_harness::Config::allow_keys);
    std::stringstream ss("[DEFAULT]\nkeyring_path="+kTestKRFile2+"\n[metadata_cache:foo]\nuser=foo\n");
    config.read(ss);

    MySQLRouter router;
    router.init_keyring(config);
    EXPECT_EQ(2, num_password_prompts);
    EXPECT_TRUE(mysql_harness::get_keyring() != nullptr);
  }
  mysql_harness::reset_keyring();
  {
    // this one should succeed completely
    mysql_harness::Config config(mysql_harness::Config::allow_keys);
    std::stringstream ss("[DEFAULT]\nkeyring_path="+kTestKRFile+"\nmaster_key_path="+kTestKRFile+".master\n[metadata_cache:foo]\nuser=foo\n");
    config.read(ss);

    MySQLRouter router;
    router.init_keyring(config);
    EXPECT_TRUE(mysql_harness::get_keyring() != nullptr);
    EXPECT_EQ(2, num_password_prompts);
  }
  mysql_harness::reset_keyring();
}
