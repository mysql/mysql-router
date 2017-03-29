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

#include "logging.h"
#include "plugin.h"

#include <cstdarg>

#include <syslog.h>

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::AppInfo;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;

class SyslogHandler : public mysql_harness::logging::Handler {
 public:
  ~SyslogHandler() { close(); }

  void open(const std::string& ident) {
    openlog(ident.c_str(), LOG_CONS | LOG_NDELAY, LOG_DAEMON);
  }

  void close() {
    closelog();
  }

 private:
  void do_log(const mysql_harness::logging::Record& record) override {
    syslog(static_cast<int>(record.level), "%s", record.message.c_str());
  }
};

std::shared_ptr<SyslogHandler> g_syslog_handler =
    std::make_shared<SyslogHandler>();

static int init(const AppInfo* info) {
  using mysql_harness::logging::register_handler;

  g_syslog_handler->open(info->program);
  register_handler(g_syslog_handler);
  return 0;
}

static int deinit(const AppInfo*) {
  g_syslog_handler->close();
  return 0;
}

Plugin syslog_plugin = {
  PLUGIN_ABI_VERSION,
  ARCHITECTURE_DESCRIPTOR,
  "Logging using syslog",
  VERSION_NUMBER(0, 0, 1),
  0, nullptr,  // Requires
  0, nullptr,  // Conflicts
  init,
  deinit,
  nullptr,  // start
  nullptr,  // stop
};
