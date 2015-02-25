#include "extension.h"

#include <string>
#include <cstdio>

static int init(Harness* harness)
{
  std::string error_log(std::string(harness->logdir) + "error.log");
  std::string general_log(std::string(harness->logdir) + "general.log");
  freopen(error_log.c_str(), "a+", stderr);
  freopen(general_log.c_str(), "a+", stdout);
}

Extension ext_info = {
  EXTENSION_VERSION,
  "Logging functions",
  0, NULL,                                      // Requires
  0, NULL,                                      // Conflicts
  init,
  NULL,
  NULL                                          // start
};
