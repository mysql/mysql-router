#include <mysql/harness/config_parser.h>
#include <mysql/harness/plugin.h>

#include "utilities.h"
#include "helpers.h"

#include <stdexcept>
#include <string>
#include <iostream>
#include <sstream>

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
  expect_equal(sections.size(), 1);
  ConfigSection& section = sections.front();

  if (section.name != "magic")
    throw std::runtime_error("Expected 'magic', got " + section.name);

  // Test that fetching a non-existing option in a section throws an
  // exception.
  expect_exception<std::runtime_error>([&]{
      section.get("my_option");
    });

  // Set the value of the option in the section
  section.set("my_option", "my_value");

  // Check that the value can be retrieved.
  const std::string value = section.get("my_option");
  if (value != "my_value")
    throw std::runtime_error("Expected 'my_value', got " + value);
}


void check_config(Config& config) {
  Config::SectionList sections = config.get("one");
  expect_equal(sections.size(), 1);
  ConfigSection& section = sections.front();
  if (section.name != "one")
    throw std::runtime_error("Expected 'one', got " + section.name);

  expect_equal(section.get("foo"), "bar");
  config.clear();
  expect_equal(config.empty(), true);
  expect_exception<bad_section>([&config]{ config.get("one"); });
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

    auto range = make_range(parse_problems, sizeof(parse_problems)/sizeof(*parse_problems));
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
          expect_equal(sections.size(), 1);
          ConfigSection& section = sections.front();
          expect_equal(section.get("foo"), "bar");
          expect_equal(config_get(&config, "one", "foo"), "bar");
          expect_equal(config_get_with_key(&config, "one", "", "foo"), "bar");
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
          expect_equal(sections.size(), 1);
          ConfigSection& section = sections.front();
          expect_equal(section.get("foo"), "bar");
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

  // Check that merging sections with mismatching names generates an
  // exception
  expect_exception<bad_section>([&one, &two]{ one.update(two); });
}

int main()
{
  try {
    test_config_basic();
    test_config_parser_basic();
    test_config_update();
  }
  catch (std::runtime_error& exc) {
    std::cerr << exc.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
