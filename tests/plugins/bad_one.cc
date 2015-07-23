#include <mysql/harness/plugin.h>

static const char* requires[] = {
  // This plugin do not exist
  "foobar",
};

static int init(const AppInfo*) {
  return 0;
}

static int deinit(const AppInfo*) {
  return 0;
}

Plugin bad_one = {
  PLUGIN_ABI_VERSION,
  "A bad plugin",
  VERSION_NUMBER(1,0,0),
  sizeof(requires)/sizeof(*requires),
  requires,
  0,
  nullptr,
  init,
  deinit,
  nullptr,
};
