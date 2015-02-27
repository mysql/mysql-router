
// Redefine the accessor to check internals
#define private public
#include "loader.h"
#undef private

#include "plugin.h"
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
  if (Plugin *ext = loader->load(name))
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


#if 0
static int check_unloading(Loader *loader,
                           const std::string& name)
{
  if (loader->unload(name))
  {
    for (auto error : loader->errors())
      std::cerr << error << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
#endif

static void test_available(Loader *loader)
{
  std::list<std::string> lst = loader->available();
  if (lst.size() != 2) {
    char buf[256];
    sprintf(buf, "Expected length 2, got %lu", lst.size());
    throw std::logic_error(buf);
  }
  if (std::find(lst.begin(), lst.end(), "example") == lst.end())
    throw std::logic_error("Missing 'example'");
  if (std::find(lst.begin(), lst.end(), "magic") == lst.end())
    throw std::logic_error("Missing 'magic'");
}

static int test_loading(Loader *loader)
{
  // Test that loading something non-existant works
  if (loader->load("test") != NULL)
    return EXIT_FAILURE;
  loader->clear_errors();

  if (int error = check_loading(loader, "example", "An example plugin"))
    return error;
  if (int error = check_loading(loader, "magic", "A magic plugin"))
    return error;

#if 0
  if (int error = check_unloading(loader, "example"))
    return EXIT_FAILURE;
  if (int error = check_unloading(loader, "magic"))
    return EXIT_FAILURE;
#endif

  return EXIT_SUCCESS;
}

static void test_init(Loader *loader)
{
  loader->init_all();
}


int main(int argc, char *argv[])
{
  const std::string prefix(dirname(argv[0]) + "/");
  std::map<std::string, std::string> params;
  params["program"] = "harness";
  params["prefix"] = prefix;

  Loader loader(prefix + "data/tests.cfg", params);
  test_available(&loader);
  if (int error = test_loading(&loader))
    exit(error);
  test_init(&loader);
  exit(EXIT_SUCCESS);
}

