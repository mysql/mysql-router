#include "common.h"

#include <sstream>
#include <string.h>

namespace mysql_harness {

std::string get_strerror(int err) {
    char msg[256];
    std::string result;

#if  !_GNU_SOURCE && (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600)
  int ret = strerror_r(err, msg, sizeof(msg));
  if (ret) {
    std::ostringstream oss;
    oss << err << " (strerror_r failed: " << ret << ")";
    result = oss.str();
  }
  else {
    result = std::string(msg);
  }
#else // GNU version
  char* ret = strerror_r(err, msg, sizeof(msg));
  result = std::string(ret);
#endif

  return result;
}

}
