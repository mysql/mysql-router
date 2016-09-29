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


#include "router_app.h"
#include "windows/main-windows.h"

#include <iostream>

int real_main(int argc, char **argv) {
  extern std::string g_program_name;
  g_program_name = argv[0];
  int result = 0;
  try {
    MySQLRouter router(argc, argv);
    // This nested try/catch block is necessary in Windows, to
    // workaround a crash that occurs when an exception is thrown from
    // a plugin (e.g. routing_plugin_tests)
    try {
      router.start();
    } catch (const std::invalid_argument &exc) {
      std::cerr << "Configuration error: " << exc.what() << std::endl;
      result = 1;
    } catch (const std::runtime_error &exc) {
	  std::cerr << "Error: " << exc.what() << std::endl;
	  result = 1;
    } catch (const silent_exception&) {}
  } catch(const std::invalid_argument &exc) {
    std::cerr << "Configuration error: " << exc.what() << std::endl;
    result = 1;
  } catch(const std::runtime_error &exc) {
    std::cerr << "Error: " << exc.what() << std::endl;
    result = 1;
  } catch (const mysql_harness::syntax_error &exc) {
    std::cerr << "Configuration syntax error: " << exc.what() << std::endl;
  } catch (const silent_exception&) {
  } catch (const std::exception &exc) {
    std::cerr << "Error: " << exc.what() << std::endl;
    result = 1;
  }
  return result;
}

int main(int argc, char **argv) {
#ifdef _WIN32
  return proxy_main(real_main, argc, argv);
#else
  return real_main(argc, argv);
#endif
}
