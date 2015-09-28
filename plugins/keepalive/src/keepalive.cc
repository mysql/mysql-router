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
 * Keepalive Plugin
 *
 * Keepalive plugin is simply sending a message every, by default,
 * 8 seconds and running until Router is shut down.
 *
 * [keepalive]
 * interval = 2
 * runs = 3
 */

#include <chrono>
#include <iostream>
#include <thread>

// Harness interface include files
#include "config_parser.h"
#include "plugin.h"
#include "logger.h"

// Keep symbols with external linkage away from global scope so that
// they do not clash with other symbols.
namespace {

const int kInterval = 60;  // in seconds
const int kRuns = 0;  // 0 means for ever

const AppInfo *app_info;

}

static int init(const AppInfo *info) {
  app_info = info;
  return 0;
}

static void start(const ConfigSection *section) {
  int interval = kInterval;
  try {
    interval = std::stoi(section->get("interval"));
  } catch (...) {
    // Anything in valid will result in using the default.
  }

  int runs = kRuns;
  try {
    runs = std::stoi(section->get("runs"));
  } catch (...) {
    // Anything in valid will result in using the default.
  }

  std::string name = section->name;
  if (!section->key.empty()) {
    name += " " + section->key;
  }

  log_info("%s started with interval %d", name.c_str(), interval);
  if (runs) {
    log_info("%s will run %d time(s)", name.c_str(), runs);
  }

  for ( int total_runs = 0 ; runs == 0 || total_runs < runs ; ++total_runs) {
    log_info(name.c_str());
    std::this_thread::sleep_for(std::chrono::seconds(interval));
  }
}

static const char *requires[] = {
  "logger",
};

Plugin keepalive = {
  PLUGIN_ABI_VERSION,
  ARCHITECTURE_DESCRIPTOR,
  "Keepalive Plugin",
  VERSION_NUMBER(0, 0, 1),
  sizeof(requires)/sizeof(*requires), requires,
  0, nullptr,
  init,
  nullptr,
  start
};
