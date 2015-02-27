#include "config_parser.h"

#include "utilities.h"
#include "helpers.h"

#include <stdexcept>
#include <string>
#include <iostream>
#include <sstream>

void test_config_basic()
{
  Config config;

  // Test that fetching a non-existing section throws an exception.
  expect_exception<std::runtime_error>([&config]{
      config.get("magic");
    });

  expect_equal(config.has("magic"), false);

  // Add the section
  config.add("magic");

  // Test that fetching a section get the right section back.
  expect_equal(config.has("magic"), true);
  ConfigSection& section = config.get("magic");
  if (section.name != "magic")
    throw std::runtime_error("Expected 'magic', got " + section.name);

  // Test that fetching a non-existing option in a section throws an
  // exception.
  expect_exception<std::runtime_error>([&config]{
      config.get("magic").get("my_option");
    });

  // Set the value of the option in the section
  config.get("magic").set("my_option", "my_value");

  // Check that the value can be retrieved.
  const std::string value = config.get("magic").get("my_option");
  if (value != "my_value")
    throw std::runtime_error("Expected 'my_value', got " + value);
}


void check_config(Config& config) {
  ConfigSection& section = config.get("one");
  if (section.name != "one")
    throw std::runtime_error("Expected 'one', got " + section.name);

  expect_equal(config.get("one").get("foo").c_str(), "bar");
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
    };

    auto range = make_range(examples, sizeof(examples)/sizeof(*examples));
    for (auto contents: range)
    {
      Config config;
      std::istringstream input(contents);
      config.read(input);
      check_config(config);
    }
  }

  // Some examples that should not work
  {
    static const char* examples[] = {
      // Unterminated section header line
      ("[one\n" "foo = bar\n"),

      // Malformed start of a section
      ("one]\n" "foo: bar\n"),

      // Options before first section
      ("  foo: bar   \n" "[one]\n"),

      // Incomplete variable interpolation
      ("[one]\n" "foo = %(bar\n"),
      ("[one]\n" "foo = %(bar)\n"),
      ("[one]\n" "foo = %(bar)sx%(foo\n"),
    };

    auto range = make_range(examples, sizeof(examples)/sizeof(*examples));
    for (auto contents: range)
    {
      Config config;
      std::istringstream input(contents);
      expect_exception<std::runtime_error>([&config, &input]{
          config.read(input);
          expect_equal(config.get("one").get("foo").c_str(), "bar");
        });
    }
  }

}


int main()
{
  try {
    test_config_basic();
    test_config_parser_basic();
  }
  catch (std::runtime_error& exc) {
    std::cerr << exc.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
