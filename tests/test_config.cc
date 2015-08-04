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

#include <mysql/harness/config_parser.h>
#include <mysql/harness/filesystem.h>
#include <mysql/harness/plugin.h>

#include "utilities.h"
#include "helpers.h"

#include <stdexcept>
#include <string>
#include <iostream>
#include <sstream>

template <>
struct TestTraits<Config>
{
  bool equal(const Config& lhs, const Config& rhs)
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

  void show_not_equal(std::ostream& out,
                      const Config& value,
                      const Config& expect)
  {
    out << "Configurations not equal\n";
    out << "\tWas: ";
    for (auto&& val: value.section_names())
      out << val.first << ":" << val.second << " ";
    out << std::endl;
    out << "\tExpected: ";
    for (auto&& val: expect.section_names())
      out << val.first << ":" << val.second << " ";
    out << std::endl;
  }
};

void test_config_basic()
{
  Config config;

  std::vector<std::string> words;
  words.push_back("reserved");
  config.set_reserved(words);

  expect_equal(config.is_reserved("reserved"), true);
  expect_equal(config.is_reserved("legal"), false);

  // A newly created configuration is always empty.
  expect_equal(config.empty(), true);

  // Test that fetching a non-existing section throws an exception.
  expect_exception<std::runtime_error>([&config]{
    config.get("magic");
  });

  expect_equal(config.has("magic"), false);

  // Add the section
  config.add("magic");

  // Test that fetching a section get the right section back.
  expect_equal(config.has("magic"), true);

  Config::SectionList sections = config.get("magic");
  expect_equal(sections.size(), 1U);
  ConfigSection* section = sections.front();

  if (section->name != "magic")
    throw std::runtime_error("Expected 'magic', got " + section->name);

  // Test that fetching a non-existing option in a section throws an
  // exception.
  expect_exception<std::runtime_error>([&]{
      section->get("my_option");
    });

  // Set the value of the option in the section
  section->set("my_option", "my_value");

  // Check that the value can be retrieved.
  const std::string value = section->get("my_option");
  if (value != "my_value")
    throw std::runtime_error("Expected 'my_value', got " + value);
}


void check_config(Config& config) {
  Config::SectionList sections = config.get("one");
  expect_equal(sections.size(), 1U);

  ConfigSection* section = sections.front();
  if (section->name != "one")
    throw std::runtime_error("Expected 'one', got " + section->name);

  expect_equal(section->get("foo"), "bar");

  // Checking that getting a non-existient option in an existing
  // section throws exception.
  expect_exception<bad_option>([&config, &section]{
      section->get("not-in-section");
    });

  config.clear();
  expect_equal(config.empty(), true);

  // Checking that getting a non-existent section throws exception
  expect_exception<bad_section>([&config]{
      config.get("one");
    });
}

void test_config_parser_basic()
{
  {
    // Some alternative versions that should give the same result
    static const char* examples[] = {
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
       "[one]\n" "foo = b%(other)s\n"),
      ("[DEFAULT]\n" "one = b\n" "two = r\n"
       "[one]\n" "foo = %(one)sa%(two)s\n"),
      ("[DEFAULT]\n" "one = b\n" "two = r\n"
       "[one:my_key]\n" "foo = %(one)sa%(two)s\n"),
    };

    auto range = make_range(examples, sizeof(examples)/sizeof(*examples));
    for (auto contents: range)
    {
      Config config(Config::allow_keys);

      std::vector<std::string> words;
      words.push_back("reserved");
      config.set_reserved(words);

      std::istringstream input(contents);
      config.read(input);
      check_config(config);
    }
  }

  // Some examples that should not work
  {
    static const char* parse_problems[] = {
      // Unterminated section header line
      ("[one\n" "foo = bar\n"),

      // Malformed start of a section
      ("one]\n" "foo: bar\n"),

      // Bad section name
      ("[one]\n" "foo = bar\n" "[mysqld]\n" "foo = baz\n"),

      // Options before first section
      ("  foo: bar   \n" "[one]\n"),

      // Incomplete variable interpolation
      ("[one]\n" "foo = %(bar\n"),
      ("[one]\n" "foo = %(bar)\n"),
      ("[one]\n" "foo = %(bar)sx%(foo\n"),

      // Unterminated last line
      ("[one]\n" "foo = bar"),

      // Repeated option
      ("[one]\n" "foo = bar\n" "foo = baz\n"),
      ("[one]\n" "foo = bar\n" "Foo = baz\n"),

      // Repeated section
      ("[one]\n" "foo = bar\n" "[one]\n" "foo = baz\n"),
      ("[one]\n" "foo = bar\n" "[ONE]\n" "foo = baz\n"),

      // Reserved words
      ("[one]\n" "mysql_trick = bar\n" "[two]\n" "foo = baz\n"),

      // Key but keys not allowed
      ("[one:my_key]\n" "foo = bar\n" "[two]\n" "foo = baz\n"),
    };

    auto range = make_range(parse_problems,
                            sizeof(parse_problems)/sizeof(*parse_problems));
    for (auto contents: range)
    {
      Config config;

      std::vector<std::string> words;
      words.push_back("mysql*");
      config.set_reserved(words);

      std::istringstream input(contents);
      expect_exception<std::exception>([&config, &input]{
          config.read(input);
          auto&& sections = config.get("one");
          expect_equal(sections.size(), 1U);
          ConfigSection* section = sections.front();
          expect_equal(section->get("foo"), "bar");
        });
    }
  }

  // Some examples where keys are allowed that should not work
  {
    static const char* parse_problems[] = {
      // Empty key
      ("[one:]\n" "foo = bar\n" "[two]\n" "foo = baz\n"),

      // Key on default section
      ("[DEFAULT:key]\n" "one = b\n" "two = r\n"
       "[one:key1]\n" "foo = %(one)sa%(two)s\n"
       "[one:key2]\n" "foo = %(one)sa%(two)s\n"),
    };

    auto range = make_range(parse_problems,
                            sizeof(parse_problems)/sizeof(*parse_problems));
    for (auto contents: range)
    {
      Config config(Config::allow_keys);

      std::istringstream input(contents);
      expect_exception<std::exception>([&config, &input] {
        config.read(input);
        auto&& sections = config.get("one");
        expect_equal(sections.size(), 1U);
        ConfigSection* section = sections.front();
        expect_equal(section->get("foo"), "bar");
      });
    }
  }
}

void test_config_update() {
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
  expect_equal(one.get("one"), "new first");
  expect_equal(one.get("two"), "second");
  expect_equal(two.get("one"), "first");

  // Non-existent options should still throw an exception
  auto&& section = config.get("one", "");
  expect_exception<bad_option>([&config, &section]{
      section.get("not-in-section");
    });

  // Check that merging sections with mismatching names generates an
  // exception
  expect_exception<bad_section>([&one, &two]{ one.update(two); });
}

void test_config_read_basic(const Path& here)
{
  // Here are three different sources of configurations that should
  // all be identical. One is a single file, one is a directory, and
  // one is a stream.

  Config dir_config = Config(Config::allow_keys);
  dir_config.read(here.join("data/logger.d"), "*.cfg");

  Config file_config = Config(Config::allow_keys);
  file_config.read(here.join("data/logger.cfg"));

  const char *const config_string =
    ("[DEFAULT]\n"
     "logdir = var/log\n"
     "etcdir = etc\n"
     "libdir = var/lib\n"
     "rundir = var/run\n"
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

  expect_equal(dir_config, file_config);
  expect_equal(dir_config, stream_config);
  expect_equal(file_config, stream_config);
}


// Here we test that reads of configuration entries overwrite previous
// read entries.
void test_config_read_overwrite(const Path& here)
{
  Config config = Config(Config::allow_keys);
  config.read(here.join("data/logger.d"), "*.cfg");
  expect_equal(config.get("magic", "").get("message"), "Some kind of");

  // Non-existent options should still throw an exception
  {
    auto&& section = config.get("magic", "");
    expect_exception<bad_option>([&config, &section]{
        section.get("not-in-section");
      });
  }

  config.read(here.join("data/magic-alt.cfg"));
  expect_equal(config.get("magic", "").get("message"), "Another message");

  // Non-existent options should still throw an exception
  {
    auto&& section = config.get("magic", "");
    expect_exception<bad_option>([&config, &section]{
        section.get("not-in-section");
      });
  }
}


int main(int, char *argv[])
{
  try {
    test_config_basic();
    test_config_parser_basic();
    test_config_update();
    test_config_read_basic(Path(argv[0]).dirname());
    test_config_read_overwrite(Path(argv[0]).dirname());
  }
  catch (std::runtime_error& exc) {
    std::cerr << exc.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
