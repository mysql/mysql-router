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

#include "config_parser.h"
#include "filesystem.h"
#include "plugin.h"

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

////////////////////////////////////////
// Third-party include files
#include "gtest/gtest.h"

////////////////////////////////////////
// Standard include files
#include <stdexcept>
#include <string>
#include <iostream>
#include <sstream>


bool operator==(const Config& lhs, const Config& rhs)
{
  // We just check the section names to start with
  auto&& lhs_names = lhs.section_names();
  auto&& rhs_names = rhs.section_names();

  // Check if the sizes differ. This is not an optimization since
  // std::equal does not work properly on ranges of unequal size.
  if (lhs_names.size() != rhs_names.size())
    return false;

  // Put the lists in vectors and sort them
  std::vector<std::pair<std::string, std::string>>
    lhs_vec(lhs_names.begin(), lhs_names.end());
  std::sort(lhs_vec.begin(), lhs_vec.end());

  std::vector<std::pair<std::string, std::string>>
    rhs_vec(rhs_names.begin(), rhs_names.end());
  std::sort(rhs_vec.begin(), rhs_vec.end());

  // Compare the elements of the sorted vectors
  return std::equal(lhs_vec.begin(), lhs_vec.end(), rhs_vec.begin());
}

void PrintTo(const Config& config, std::ostream& out) {
  for (auto&& val: config.section_names())
    out << val.first << ":" << val.second << " ";
}

class ConfigTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    std::vector<std::string> words;
    words.push_back("reserved");
    config.set_reserved(words);
  }

  Config config;
};

Path g_here;

TEST_F(ConfigTest, TestEmpty)
{
  EXPECT_TRUE(config.is_reserved("reserved"));
  EXPECT_FALSE(config.is_reserved("legal"));

  // A newly created configuration is always empty.
  EXPECT_TRUE(config.empty());

  // Test that fetching a non-existing section throws an exception.
  EXPECT_THROW(config.get("magic"), std::runtime_error);

  EXPECT_FALSE(config.has("magic"));
}

TEST_F(ConfigTest, SetGetTest) {
  // Add the section
  config.add("magic");

  // Test that fetching a section get the right section back.
  EXPECT_TRUE(config.has("magic"));

  Config::SectionList sections = config.get("magic");
  EXPECT_EQ(1U, sections.size());

  ConfigSection* section = sections.front();
  EXPECT_EQ("magic", section->name);

  // Test that fetching a non-existing option in a section throws a
  // run-time exception.
  EXPECT_THROW(section->get("my_option"), std::runtime_error);

  // Set the value of the option in the section
  section->set("my_option", "my_value");

  // Check that the value can be retrieved.
  EXPECT_EQ("my_value", section->get("my_option"));

  config.clear();
  EXPECT_TRUE(config.empty());
}


class GoodParseTestAllowKey
  : public ::testing::TestWithParam<const char*> 
{
protected:
  virtual void SetUp() {
    config = new Config(Config::allow_keys);

    std::vector<std::string> words;
    words.push_back("reserved");
    config->set_reserved(words);

    std::istringstream input(GetParam());
    config->read(input);
  }

  virtual void TearDown() {
    delete config;
    config = nullptr;
  }

  Config *config;
};

TEST_P(GoodParseTestAllowKey, SectionOne) {
  // Checking that getting a non-existent section throws exception
  EXPECT_THROW(config->get("nonexistant-section"), bad_section);

  Config::SectionList sections = config->get("one");
  EXPECT_EQ(1U, sections.size());

  ConfigSection* section = sections.front();
  EXPECT_EQ("one", section->name);
  EXPECT_EQ("bar", section->get("foo"));

  // Checking that getting a non-existient option in an existing
  // section throws exception.
  EXPECT_THROW(section->get("nonexistant-option"), bad_option);
}

const char *good_examples[] = {
  ("[one]\n" "foo = bar\n"),
  ("[one]\n" "foo: bar\n"),
  (" [one]   \n" "  foo: bar   \n"),
  (" [one]\n" "  foo   :bar   \n"),
  ("# Hello\n"
   " [one]\n" "  foo   :bar   \n"),
  ("# Hello\n"
   "# World!\n"
   " [one]\n" "  foo   :bar   \n"),
  ("; Hello\n"
   " [one]\n" "  foo   :bar   \n"),
  ("[DEFAULT]\n" "foo = bar\n"
   "[one]\n"),
  ("[DEFAULT]\n" "other = ar\n"
   "[one]\n" "foo = b{other}\n"),
  ("[DEFAULT]\n" "one = b\n" "two = r\n"
   "[one]\n" "foo = {one}a{two}\n"),
  ("[DEFAULT]\n" "one = b\n" "two = r\n"
   "[one:my_key]\n" "foo = {one}a{two}\n")
};

INSTANTIATE_TEST_CASE_P(TestParsing, GoodParseTestAllowKey,
                        ::testing::ValuesIn(good_examples));

class BadParseTestForbidKey
  : public ::testing::TestWithParam<const char*> 
{
protected:
  virtual void SetUp() {
    config = new Config;

    std::vector<std::string> words;
    words.push_back("reserved");
    config->set_reserved(words);
  }

  virtual void TearDown() {
    delete config;
    config = nullptr;
  }

  Config *config;
};

TEST_P(BadParseTestForbidKey, SyntaxError) {
  EXPECT_ANY_THROW(config->read(GetParam()));
}

static const char* syntax_problems[] = {
  // Unterminated section header line
  ("[one\n" "foo = bar\n"),

  // Malformed start of a section
  ("one]\n" "foo: bar\n"),

  // Bad section name
  ("[one]\n" "foo = bar\n" "[mysqld]\n" "foo = baz\n"),

  // Options before first section
  ("  foo: bar   \n" "[one]\n"),

  // Incomplete variable interpolation
  ("[one]\n" "foo = {bar"),
  ("[one]\n" "foo = {bar\n"),
  ("[one]\n" "foo = {bar}x{foo"),
  ("[one]\n" "foo = {bar}x{foo\n"),

  // Unterminated last line
  ("[one]\n" "foo = bar"),
  ("[one]\n" "foo = bar\\"),

  // Repeated option
  ("[one]\n" "foo = bar\n" "foo = baz\n"),
  ("[one]\n" "foo = bar\n" "Foo = baz\n"),

  // Space in option
  ("[one]\n" "foo bar = bar\n" "bar = baz\n"),

  // Repeated section
  ("[one]\n" "foo = bar\n" "[one]\n" "foo = baz\n"),
  ("[one]\n" "foo = bar\n" "[ONE]\n" "foo = baz\n"),

  // Reserved words
  ("[one]\n" "mysql_trick = bar\n" "[two]\n" "foo = baz\n"),

  // Key but keys not allowed
  ("[one:my_key]\n" "foo = bar\n" "[two]\n" "foo = baz\n"),
};

INSTANTIATE_TEST_CASE_P(TestParsingSyntaxError, BadParseTestForbidKey,
                        ::testing::ValuesIn(syntax_problems));

class BadParseTestAllowKeys
  : public ::testing::TestWithParam<const char*> 
{
protected:
  virtual void SetUp() {
    config = new Config(Config::allow_keys);

    std::vector<std::string> words;
    words.push_back("reserved");
    config->set_reserved(words);
  }

  virtual void TearDown() {
    delete config;
    config = nullptr;
  }

  Config *config;
};

TEST_P(BadParseTestAllowKeys, SemanticError) {
  EXPECT_ANY_THROW(config->read(GetParam()));
}

static const char* semantic_problems[] = {
  // Empty key
  ("[one:]\n" "foo = bar\n" "[two]\n" "foo = baz\n"),

  // Key on default section
  ("[DEFAULT:key]\n" "one = b\n" "two = r\n"
   "[one:key1]\n" "foo = {one}a{two}\n"
   "[one:key2]\n" "foo = {one}a{two}\n"),
};

INSTANTIATE_TEST_CASE_P(TestParseErrorAllowKeys, BadParseTestAllowKeys,
                        ::testing::ValuesIn(semantic_problems));

TEST(TestConfig, ConfigUpdate) {
  const char *const configs[] = {
    ("[one]\n"
     "one = first\n"
     "two = second\n"),
    ("[one]\n"
     "one = new first\n"
     "[two]\n"
     "one = first\n"),
  };

  Config config(Config::allow_keys);
  std::istringstream input(configs[0]);
  config.read(input);

  Config other(Config::allow_keys);
  std::istringstream other_input(configs[1]);
  other.read(other_input);

  Config expected(Config::allow_keys);
  config.update(other);

  ConfigSection& one = config.get("one", "");
  ConfigSection& two = config.get("two", "");
  EXPECT_EQ("new first", one.get("one"));
  EXPECT_EQ("second", one.get("two"));
  EXPECT_EQ("first", two.get("one"));

  // Non-existent options should still throw an exception
  auto&& section = config.get("one", "");
  EXPECT_THROW(section.get("nonexistant-option"), bad_option);

  // Check that merging sections with mismatching names generates an
  // exception
  EXPECT_THROW(one.update(two), bad_section);
}

TEST(TestConfig, ConfigReadBasic)
{
  // Here are three different sources of configurations that should
  // all be identical. One is a single file, one is a directory, and
  // one is a stream.

  Config dir_config = Config(Config::allow_keys);
  dir_config.read(g_here.join("data/logger.d"), "*.cfg");

  Config file_config = Config(Config::allow_keys);
  file_config.read(g_here.join("data/logger.cfg"));

  const char *const config_string =
    ("[DEFAULT]\n"
     "logging_folder = var/log\n"
     "config_folder = etc\n"
     "plugin_folder = var/lib\n"
     "runtime_folder = var/run\n"
     "[logger]\n"
     "library = logger\n"
     "[example]\n"
     "library = example\n"
     "[magic]\n"
     "library = magic\n"
     "message = Some kind of\n");

  Config stream_config(Config::allow_keys);
  std::istringstream stream_input(config_string);
  stream_config.read(stream_input);

  EXPECT_EQ(dir_config, file_config);
  EXPECT_EQ(dir_config, stream_config);
  EXPECT_EQ(file_config, stream_config);
}


// Here we test that reads of configuration entries overwrite previous
// read entries.
TEST(TestConfig, ConfigReadOverwrite)
{
  Config config = Config(Config::allow_keys);
  config.read(g_here.join("data/logger.d"), "*.cfg");
  EXPECT_EQ("Some kind of", config.get("magic", "").get("message"));

  // Non-existent options should still throw an exception
  {
    auto&& section = config.get("magic", "");
    EXPECT_THROW(section.get("not-in-section"), bad_option);
  }

  config.read(g_here.join("data/magic-alt.cfg"));
  EXPECT_EQ("Another message", config.get("magic", "").get("message"));

  // Non-existent options should still throw an exception
  {
    auto&& section = config.get("magic", "");
    EXPECT_THROW(section.get("not-in-section"), bad_option);
  }
}


int main(int argc, char *argv[])
{
  g_here = Path(argv[0]).dirname();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
