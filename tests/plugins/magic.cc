#include "plugin.h"

#include <cstdlib>
#include <iostream>

Info* g_info;

static int init(Info* info) {
  g_info = info;
  return 0;
}

void do_magic() {
  std::cout << config_get(g_info->config, "magic", "message") << std::endl;
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
