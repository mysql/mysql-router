/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "common.h"
#include "dim.h"
#include "utils.h"
#include "mysql_session.h"
#include "router_app.h"
#include "random_generator.h"
#include "windows/main-windows.h"
#include <mysql.h>
#include <iostream>

/** @brief Initialise Dependency Injection Manager (DIM)
 *
 * This is the place to initialise all the DI stuff used thoroughout our application.
 * (well, maybe we'll want plugins to init their own stuff, we'll see).
 *
 * Naturally, unit tests will not run this code, as they will initialise the objects
 * they need their own way.
 */
static void init_DIM() {
  mysql_harness::DIM& dim = mysql_harness::DIM::instance();

  // MySQLSession
  dim.set_MySQLSession([](){ return new mysqlrouter::MySQLSession(); });

  // Ofstream
  dim.set_Ofstream([](){ return new mysqlrouter::RealOfstream(); });
}

int real_main(int argc, char **argv) {
  mysql_harness::rename_thread("main");
  init_DIM();

  extern std::string g_program_name;
  g_program_name = argv[0];
  int result = 0;

  if (mysql_library_init(argc, argv, NULL)) {
    std::cerr << "Could not initialize MySQL library\n";
    return 1;
  }

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

  // We should deinitialize mysql-lib but we can't do it safely here until
  // we do WL9558 "Plugin life-cycle that support graceful shutdown and restart."
  // Currently we can get here while there are still some threads running
  // (like metadata_cache thread that is managed by the global g_metadata_cache)
  // that still use mysql-lib, which leads to crash.
  //mysql_library_end();

  return result;
}

int main(int argc, char **argv) {

  mysql_harness::DIM& dim = mysql_harness::DIM::instance();
  dim.set_RandomGenerator(
    [](){ static mysql_harness::RandomGenerator rg; return &rg; },
    [](mysql_harness::RandomGeneratorInterface*){}  // don't delete our static!
  );
#ifdef _WIN32
  return proxy_main(real_main, argc, argv);
#else
  return real_main(argc, argv);
#endif
}
