/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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
 * @file
 *
 * MySQL Harness is an extensible framework that handles loading and
 * unloading of extensions. The built-in features is dependency
 * tracking between extensions, configuration file handling, and
 * support for extension life-cycles.
 */

#include "mysql/harness/arg_handler.h"
#include "mysql/harness/loader.h"

#include "utilities.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

using mysql_harness::Path;
using mysql_harness::Loader;
using mysql_harness::utility::strip_copy;
using mysql_harness::utility::basename;

static void
print_usage_and_exit(const CmdArgHandler& handler,
                     const std::string& program,
                     const std::string &message = "") {
  if (message.length() > 0) {
    std::cerr << message << std::endl;
  }
  for (auto& it : handler.usage_lines("usage: " + program, "config file", 72)) {
    std::cerr << it << std::endl;
  }
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  const std::string program(basename(argv[0]));
  CmdArgHandler handler(true);
  std::map<std::string, std::string> params;
  std::string config_file;
  bool console = false;

  params["program"] = program;

  handler.add_option(CmdOption::OptionNames({"-h", "--help"}),
                     "Show help screen",
                     CmdOptionValueReq::none,
                     "",
                     [&handler, program](const std::string &) {
                       print_usage_and_exit(handler, program);
                     });

  auto param_action = [&params, &handler, program](const std::string &value) {
    auto pos = value.find("=");
    if (pos == std::string::npos) {
      print_usage_and_exit(handler, program, "Incorrectly formatted parameter");
    }
    params[value.substr(0, pos)] = strip_copy(value.substr(pos + 1));
  };

  handler.add_option(CmdOption::OptionNames({"-p", "--param"}),
                     "Set parameter <name> to <value>",
                     CmdOptionValueReq::required, "name=value", param_action);

  handler.add_option(CmdOption::OptionNames({"--console"}),
                     "Print log to console",
                     CmdOptionValueReq::none,
                     "",
                     [&console](const std::string&) {
                       console = true;
                     });

  try {
    handler.process({argv + 1, argv + argc});
  } catch (const std::runtime_error& err) {
    print_usage_and_exit(handler, program, err.what());
  } catch (const std::invalid_argument& err) {
    print_usage_and_exit(handler, program, err.what());
  }

  try {
    config_file.assign(handler.get_rest_arguments().at(0));
  } catch (const std::out_of_range) {
    print_usage_and_exit(handler, program, "No configuration file provided");
  }

  if (console) {
    params["logging_folder"] = "";
  }

  try {
    mysql_harness::Loader loader(program, params);
    loader.read(config_file);
    loader.start();
  }
  catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
