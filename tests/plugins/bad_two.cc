#include <mysql/harness/plugin.h>

static const char* requires[] = {
  // Magic plugin is version 1.2.3, so version does not match and this
  // should fail to load.
  "magic (>>1.2.3)",
};

static int init(const AppInfo*) {
  return 0;
}

static int deinit(const AppInfo*) {
  return 0;
}

Plugin bad_two = {
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
