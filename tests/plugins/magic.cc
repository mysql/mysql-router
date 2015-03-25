#include <mysql/harness/plugin.h>
#include <mysql/harness/logger.h>

#include <cstdlib>
#include <iostream>

AppInfo* g_info;

static int init(AppInfo* info) {
  g_info = info;
  return 0;
}

void do_magic() {
  log_info("%s", config_get(g_info->config, "magic", "message"));
}

Plugin magic = {
  PLUGIN_ABI_VERSION,
  "A magic plugin",
  VERSION_NUMBER(1,2,3),
  0,
  NULL,
  0,
  NULL,
  init,
  NULL,
};
