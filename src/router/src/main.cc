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

#include <iostream>

int main(int argc, char **argv) {
  try {
    MySQLRouter router(argc, argv);
    router.start();
  } catch(const std::invalid_argument &exc) {
    std::cerr << "Configuration error: " << exc.what() << std::endl;
    return 1;
  } catch(const std::runtime_error &exc) {
    std::cerr << "Error: " << exc.what() << std::endl;
    return 1;
  } catch (const syntax_error &exc) {
    std::cerr << "Configuration syntax error: " << exc.what() << std::endl;
  }

  return 0;
}
