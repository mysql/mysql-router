#include "utilities.h"

#include "helpers.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>


static int test_iterator() {
  static const char* array[] = {
    "one", "two", "three",
  };
  const int array_length = sizeof(array)/sizeof(*array);
  const char** ptr = array;

  auto range = make_range(array, array_length);
  for (auto elem: range) {
    expect_equal(elem, *ptr);
    expect_less(ptr - array,
                 static_cast<long>(sizeof(array)/sizeof(*array)));
    ++ptr;
  }
}


int main()
{
  try {
    test_iterator();
  }
  catch (std::runtime_error& exc) {
    std::cerr << exc.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
