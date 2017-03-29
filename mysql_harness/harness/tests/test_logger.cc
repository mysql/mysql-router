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

#include "logger.h"

#include "mysql/harness/filesystem.h"

////////////////////////////////////////
// Internal interfaces
#include "mysql/harness/loader.h"

#include "logger.h"
#include "logging_registry.h"

////////////////////////////////////////
// Third-party include files
#include "gmock/gmock.h"
#include "gtest/gtest.h"

////////////////////////////////////////
// Standard include files
#include <stdexcept>

using mysql_harness::Path;
using mysql_harness::logging::FileHandler;
using mysql_harness::logging::LogLevel;
using mysql_harness::logging::Logger;
using mysql_harness::logging::Record;
using mysql_harness::logging::StreamHandler;
using mysql_harness::logging::create_logger;
using mysql_harness::logging::log_debug;
using mysql_harness::logging::log_error;
using mysql_harness::logging::log_info;
using mysql_harness::logging::log_warning;
using mysql_harness::logging::remove_logger;


using testing::EndsWith;
using testing::Eq;
using testing::Ge;
using testing::Gt;
using testing::HasSubstr;
using testing::StartsWith;

Path g_here;

TEST(TestBasic, Setup) {
  // Test that creating a logger will give it a name and a default log
  // level.
  Logger logger("my_module");
  EXPECT_EQ(logger.get_name(), "my_module");
  EXPECT_EQ(logger.get_level(), LogLevel::kWarning);

  logger.set_level(LogLevel::kDebug);
  EXPECT_EQ(logger.get_level(), LogLevel::kDebug);
}

class LoggingTest : public ::testing::Test {
 public:
  // Here we are just testing that messages are written and in the
  // right format, so we use Debug log level, which will print all
  // messages.
  Logger logger{"my_module", LogLevel::kDebug};
};

TEST_F(LoggingTest, StreamHandler) {
  std::stringstream buffer;
  logger.add_handler(std::make_shared<StreamHandler>(buffer));

  ASSERT_THAT(buffer.tellp(), Eq(0));
  logger.handle(Record{LogLevel::kInfo, getpid(), 0, "my_module", "Message"});
  EXPECT_THAT(buffer.tellp(), Gt(0));
  EXPECT_THAT(buffer.str(), StartsWith("1970-01-01 01:00:00 my_module INFO"));
  EXPECT_THAT(buffer.str(), EndsWith("Message\n"));
}

TEST_F(LoggingTest, FileHandler) {
  // Check that an exception is thrown for a path that cannot be
  // opened.
  EXPECT_ANY_THROW(FileHandler("/something/very/unlikely/to/exist"));

  // We do not use mktemp or friends since we want this to work on
  // Windows as well.
  Path log_file(g_here.join("log4-" + std::to_string(getpid()) + ".log"));
  logger.add_handler(std::make_shared<FileHandler>(log_file));

  // Log one record
  logger.handle(Record{LogLevel::kInfo, getpid(), 0, "my_module", "Message"});

  // Open and read the entire file into memory.
  std::vector<std::string> lines;
  {
    std::ifstream ifs_log(log_file.str());
    std::string line;
    while (std::getline(ifs_log, line))
      lines.push_back(line);
  }

  // We do the assertion here to ensure that we can do as many tests
  // as possible and report issues.
  ASSERT_THAT(lines.size(), Ge(1));

  // Check basic properties for the first line.
  EXPECT_THAT(lines.size(), Eq(1));
  EXPECT_THAT(lines.at(0), StartsWith("1970-01-01 01:00:00 my_module INFO"));
  EXPECT_THAT(lines.at(0), EndsWith("Message"));
}

TEST_F(LoggingTest, Messages) {
  std::stringstream buffer;
  logger.add_handler(std::make_shared<StreamHandler>(buffer));

  time_t now;
  time(&now);

  auto pid = getpid();

  auto check_message = [this, &buffer, now, pid](
      const std::string& message, LogLevel level,
      const std::string& level_str) {
    buffer.str("");
    ASSERT_THAT(buffer.tellp(), Eq(0));

    Record record{level, pid, now, "my_module", message};
    logger.handle(record);

    EXPECT_THAT(buffer.str(), EndsWith(message + "\n"));
    EXPECT_THAT(buffer.str(), HasSubstr(level_str));
  };

  check_message("Crazy noodles", LogLevel::kError, " ERROR ");
  check_message("Sloth tantrum", LogLevel::kWarning, " WARNING ");
  check_message("Russel's teapot", LogLevel::kInfo, " INFO ");
  check_message("Bugs galore", LogLevel::kDebug, " DEBUG ");
}

// Check that messages are not emitted when the level is set higher.
TEST_F(LoggingTest, Level) {
  std::stringstream buffer;
  logger.add_handler(std::make_shared<StreamHandler>(buffer));

  time_t now;
  time(&now);

  auto pid = getpid();

  auto check_level = [this, &buffer, now, pid](LogLevel level) {
    // Set the log level of the logger.
    logger.set_level(level);

    // Some handy shorthands for the levels as integers.
    const int limit_level = static_cast<int>(level) + 1;
    const int max_level = static_cast<int>(LogLevel::kDebug);

    // Loop over all levels below or equal to the provided level and
    // make sure that something is printed.
    for (int lvl = 0 ; lvl < limit_level ; ++lvl) {
      buffer.str("");
      ASSERT_THAT(buffer.tellp(), Eq(0));
      logger.handle(Record{
          static_cast<LogLevel>(lvl), pid, now, "my_module", "Some message"});
      auto output = buffer.str();
      EXPECT_THAT(output.size(), Gt(0));
    }

    // Loop over all levels above the provided level and make sure
    // that nothing is printed.
    for (int lvl = limit_level ; lvl <= max_level ; ++lvl) {
      buffer.str("");
      ASSERT_THAT(buffer.tellp(), Eq(0));
      logger.handle(Record{
          static_cast<LogLevel>(lvl), pid, now, "my_module", "Some message"});
      auto output = buffer.str();
      EXPECT_THAT(output.size(), Eq(0));
    }
  };

  check_level(LogLevel::kFatal);
  check_level(LogLevel::kError);
  check_level(LogLevel::kWarning);
  check_level(LogLevel::kInfo);
  check_level(LogLevel::kDebug);
}

////////////////////////////////////////////////////////////////
// Tests of the functional interface to the logger.
////////////////////////////////////////////////////////////////

TEST(FunctionalTest, CreateRemove) {
  // Test that creating two modules with different names succeed.
  EXPECT_NO_THROW(create_logger("my_first"));
  EXPECT_NO_THROW(create_logger("my_second"));

  // Test that trying to create two loggers for the same module fails.
  EXPECT_THROW(create_logger("my_first"), std::logic_error);
  EXPECT_THROW(create_logger("my_second"), std::logic_error);

  // Check that we can remove one of the modules and that removing it
  // a second time fails (mostly to get full coverage).
  ASSERT_NO_THROW(remove_logger("my_second"));
  EXPECT_THROW(remove_logger("my_second"), std::logic_error);

  // Clean up after the tests
  ASSERT_NO_THROW(remove_logger("my_first"));
}

void expect_no_log(void (*func)(const char*, const char*, ...),
                   std::stringstream& buffer, const char* module) {
  // Clear the buffer first and ensure that it was cleared to avoid
  // triggering other errors.
  buffer.str("");
  ASSERT_THAT(buffer.tellp(), Eq(0));

  // Write a simple message with a variable
  const int x = 3;
  func(module, "Just a test of %d", x);

  // Log should be empty
  EXPECT_THAT(buffer.tellp(), Eq(0));
}

void expect_log(void (*func)(const char*, const char*, ...),
                std::stringstream& buffer, const char* module,
                const char* kind) {
  // Clear the buffer first and ensure that it was cleared to avoid
  // triggering other errors.
  buffer.str("");
  ASSERT_THAT(buffer.tellp(), Eq(0));

  // Write a simple message with a variable
  const int x = 3;
  func(module, "Just a test of %d", x);

  auto log = buffer.str();

  // Check that only one line was generated for the message. If the
  // message was sent to more than one logger, it could result in
  // multiple messages.
  EXPECT_THAT(std::count(log.begin(), log.end(), '\n'), Eq(1));

  // Check that the log contain the (expanded) message, the correct
  // indication (e.g., ERROR or WARNING), and the module name.
  EXPECT_THAT(log, HasSubstr("Just a test of 3"));
  EXPECT_THAT(log, HasSubstr(kind));
  EXPECT_THAT(log, HasSubstr(module));
}

TEST(FunctionalTest, Handlers) {
  // The loader create these modules during start, so tests of the
  // logger that involve the loader are inside the loader unit
  // test. Here we instead call these functions directly.
  ASSERT_NO_THROW(create_logger("my_first"));
  ASSERT_NO_THROW(create_logger("my_second"));

  std::stringstream buffer;
  register_handler(std::make_shared<StreamHandler>(buffer));

  set_log_level(LogLevel::kDebug);
  expect_log(log_error, buffer, "my_first", "ERROR");
  expect_log(log_warning, buffer, "my_first", "WARNING");
  expect_log(log_info, buffer, "my_first", "INFO");
  expect_log(log_debug, buffer, "my_first", "DEBUG");

  set_log_level(LogLevel::kError);
  expect_log(log_error, buffer, "my_first", "ERROR");
  expect_no_log(log_warning, buffer, "my_first");
  expect_no_log(log_info, buffer, "my_first");
  expect_no_log(log_debug, buffer, "my_first");

  set_log_level(LogLevel::kWarning);
  expect_log(log_error, buffer, "my_first", "ERROR");
  expect_log(log_warning, buffer, "my_first", "WARNING");
  expect_no_log(log_info, buffer, "my_first");
  expect_no_log(log_debug, buffer, "my_first");
}

int main(int argc, char *argv[]) {
  g_here = Path(argv[0]).dirname();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
