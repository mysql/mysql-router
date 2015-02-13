#include "extension.h"

#include <iostream>
#include <cstdlib>

#include <unistd.h>

static const char* requires[] = {
  "magic.so"
};

static int init(Harness* harness) {
  extern void do_magic();
  do_magic();
  return 0;
}

static int deinit(Harness* harness) {
}

static void *start(Harness* harness) {
  for (int x = 0 ; x < 10 ; ++x) {
    std::cout << "<count: " << x << ">" << std::endl;
    sleep(1);
  }
  return NULL;
}

Extension ext_info = {
  EXTENSION_VERSION,
  "An example plugin",
  sizeof(requires)/sizeof(*requires),
  requires,
  0,
  NULL,
  init,
  deinit,
  start,
};

