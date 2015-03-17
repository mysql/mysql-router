#include "plugin.h"

static const char* requires[] = {
  // This plugin do not exist
  "foobar",
};

static int init(AppInfo* info) {
  return 0;
}

static int deinit(AppInfo* info) {
  return 0;
}

Plugin bad_one = {
  PLUGIN_ABI_VERSION,
  "A bad plugin",
  VERSION_NUMBER(1,0,0),
  sizeof(requires)/sizeof(*requires),
  requires,
  0,
  NULL,
  init,
  deinit,
};
