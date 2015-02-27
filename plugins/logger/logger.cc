#include "plugin.h"

#include <string>
#include <cstdio>

static int init(Harness* harness)
{
  std::string error_log(std::string(harness->logdir) + "error.log");
  std::string general_log(std::string(harness->logdir) + "general.log");
  freopen(error_log.c_str(), "a+", stderr);
  freopen(general_log.c_str(), "a+", stdout);
  return 0;
}

Plugin ext_info = {
  PLUGIN_ABI_VERSION,
  "Logging functions",
  0, NULL,                                      // Requires
  0, NULL,                                      // Conflicts
  init,
  NULL,
  NULL                                          // start
};
