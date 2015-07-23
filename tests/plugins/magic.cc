#include <mysql/harness/plugin.h>
#include <mysql/harness/logger.h>
#include <mysql/harness/config_parser.h>

#include <cstdlib>
#include <iostream>

const AppInfo* g_info;
const ConfigSection* g_section;

static int init(const AppInfo* info) {
  g_info = info;
  return 0;
}

void do_magic() {
  auto&& section = g_info->config->get("magic", "");
  auto&& message = section.get("message");
  log_info("%s", message.c_str());
}

Plugin magic = {
  PLUGIN_ABI_VERSION,
  "A magic plugin",
  VERSION_NUMBER(1,2,3),
  0,
  nullptr,
  0,
  nullptr,
  init,
  nullptr,                                      // deinit
  nullptr,                                      // start
};
