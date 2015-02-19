
// Redefine the accessor to check internals
#define private public
#include "loader.h"
#undef private

#include "extension.h"
#include "utilities.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <algorithm>

static int check_loading(Loader *loader,
                         const std::string& name,
                         const std::string& brief)
{
  if (Extension *ext = loader->load(name))
  {
    if (ext->brief != brief)
      return EXIT_FAILURE;
  }
  else
  {
    for (auto error : loader->errors())
      std::cerr << error << std::endl;
    loader->clear_errors();
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}


static int check_unloading(Loader *loader,
                           const std::string& name)
{
  if (int err = loader->unload(name))
  {
    for (auto error : loader->errors())
      std::cerr << error << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}


static void test_available(Loader *loader)
{
  std::list<std::string> lst;
  if (int err = loader->available(&lst))
    throw std::logic_error("Loader::available got error");
  if (lst.size() != 2) {
    char buf[256];
    sprintf(buf, "Expected length 2, got %lu", lst.size());
    throw std::logic_error(buf);
  }
  if (std::find(lst.begin(), lst.end(), "example.so") == lst.end())
    throw std::logic_error("Missing 'example.so'");
  if (std::find(lst.begin(), lst.end(), "magic.so") == lst.end())
    throw std::logic_error("Missing 'magic.so'");
}

static int test_loading(Loader *loader)
{
  // Test that loading something non-existant works
  if (loader->load("test.so") != NULL)
    return EXIT_FAILURE;
  loader->clear_errors();
  std::list<std::string> lst;
  if (int err = loader->available(&lst))
    throw std::logic_error("Loader::available got error");
  for (auto candidate : lst)
    std::cerr << "candidate: " << candidate << std::endl;

  if (int error = check_loading(loader, "example.so", "An example plugin"))
    return error;
  if (int error = check_loading(loader, "magic.so", "A magic plugin"))
    return error;

#if 0
  if (int error = check_unloading(loader, "example.so"))
    return EXIT_FAILURE;
  if (int error = check_unloading(loader, "magic.so"))
    return EXIT_FAILURE;
#endif

  return EXIT_SUCCESS;
}

static int test_init(Loader *loader)
{
  return loader->init_extensions();
}


int main(int argc, char *argv[])
{
  Loader loader(dirname(argv[0]) + "/", "harness");
  test_available(&loader);
  if (int error = test_loading(&loader))
    exit(error);
  if (int error = test_init(&loader))
    exit(error);
  exit(EXIT_SUCCESS);
}

