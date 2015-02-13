#include "extension.h"

#include <cstdlib>
#include <iostream>

static int init(Harness* harness) {
  return 0;
}

void do_magic() {
  std::cout << "Just a test" << std::endl;
}

Extension ext_info = {
  EXTENSION_VERSION,
  "A magic plugin",
  0,
  NULL,
  0,
  NULL,
  init,
  NULL,
};
