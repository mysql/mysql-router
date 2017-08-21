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

#include "mysql/harness/logging/handler.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/plugin.h"

#include <cstdarg>

#include <syslog.h>

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::AppInfo;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;
using mysql_harness::logging::LogLevel;

class SyslogHandler final : public mysql_harness::logging::Handler {
 public:
  static constexpr const char* kDefaultName = "syslog";

  SyslogHandler(bool format_messages = true, LogLevel level = LogLevel::kNotSet)
      : mysql_harness::logging::Handler(format_messages, level) {}
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

static void init(mysql_harness::PluginFuncEnv* env) {
  const AppInfo* info = get_app_info(env);
  using mysql_harness::logging::register_handler;

  g_syslog_handler->open(info->program);
  register_handler(SyslogHandler::kDefaultName, g_syslog_handler);
}

static void deinit(mysql_harness::PluginFuncEnv*) {
  g_syslog_handler->close();
}

extern "C" {
  Plugin harness_plugin_syslog = {
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
}
