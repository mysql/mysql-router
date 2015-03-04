#include "plugin.h"

#include <cstdlib>
#include <iostream>

static int init(Info* info) {
  return 0;
}

void do_magic() {
  std::cout << "Just a test" << std::endl;
}

Plugin magic = {
  PLUGIN_ABI_VERSION,
  "A magic plugin",
  0,
  NULL,
  0,
  NULL,
  init,
  NULL,
};
