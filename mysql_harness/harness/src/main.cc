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

/**
 * @mainpage
 *
 * MySQL Harness is an extensible framework that handles loading and
 * unloading of extensions. The built-in features is dependency
 * tracking between extensions, configuration file handling, and
 * support for extension life-cycles.
 */

#include "filesystem.h"
#include "loader.h"
#include "utilities.h"

#include <getopt.h>

#include <iostream>
#include <cstring>
#include <string>

static void
print_usage_and_exit(int, char *argv[],
                     const std::string& message = "")
{
  const Path program = Path(argv[0]).basename();

  if (message.length() > 0)
    std::cerr << message << std::endl;
  std::cerr << "Usage: " << program << " <options> <config-file>\n"
            << "   --param <name>=<value>   Set parameter <name> to <value>\n"
            << "   --console                Print log to console\n"
            << std::endl;
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
  const Path program = Path(argv[0]).basename();

  std::map<std::string, std::string> params;
  params["program"] = program.str();

  std::string config_file;

  bool opt_console = false;
  static struct option options[] = {
    { "param", required_argument, 0, 'p' },
    { "console", no_argument, 0,  'c' },
    { 0, 0, 0, 0 }
  };

  int index, opt;
  while ((opt = getopt_long(argc, argv, "cp:", options, &index)) != -1)
  {
    std::string key, value;
    const char *end;
    switch (opt)
    {
    case 0:
      // Options automatically set
      break;

    case 'p':
      if (!(end = strchr(optarg, '=')))
        print_usage_and_exit(argc, argv,
                             "Incorrectly formatted parameter");
      key.assign<const char*>(optarg, end);
      value.assign(end + 1);
      params[key] = value;
      break;

    case 'c':
      opt_console = true;
      break;

    case '?':
    default:
      print_usage_and_exit(argc, argv);
      break;
    }
  }

  if (optind < argc)
  {
    if (optind + 1 != argc)
      print_usage_and_exit(argc, argv,
                           "Too many arguments provided");
    config_file = argv[optind];
  }
  else
    print_usage_and_exit(argc, argv,
                         "No configuration file provided");

  if (opt_console)
    params["logging_folder"] = "";

  try {
    Loader loader(program.str(), params);
    loader.read(config_file);
    loader.start();
  }
  catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
