#include "utilities.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

static int test_iterator() {
  static const char* array[] = {
    "one", "two", "three",
  };
  const int array_length = sizeof(array)/sizeof(*array);
  const char** ptr = array;

  auto range = make_range(array, array_length);
  for (auto elem : range) {
    if (strcmp(elem, *ptr) != 0)
      return EXIT_FAILURE;
    if (ptr - array > sizeof(array)/sizeof(*array))
      return EXIT_FAILURE;
    ++ptr;
  }
}


int main()
{
  if (int error = test_iterator())
    exit(error);
  exit(EXIT_SUCCESS);
}
