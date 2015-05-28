
// Redefine the accessor to check internals
#define private public
#include <mysql/harness/loader.h>
#undef private

#include <mysql/harness/plugin.h>

#include "utilities.h"
#include "exception.h"
#include "helpers.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using std::string;
using std::vector;
using std::cout;
using std::endl;

template <class Checks>
void check_loading(Loader *loader,
                   const string& name,
                   Checks extra)
{
  Plugin *ext = loader->load(name);
  if (ext == nullptr)
    throw std::runtime_error("Plugin '" + name + "' cannot be loaded");
  extra(*ext);
}


#if 0
static int check_unloading(Loader *loader,
                           const string& name)
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
  const long int expected = 5;
  vector<string> lst = loader->available();
  if (lst.size() != expected) {
    char buf[256];
    sprintf(buf, "Expected length %lu, got %lu", expected, lst.size());
    throw std::logic_error(buf);
  }
  if (std::find(lst.begin(), lst.end(), "example") == lst.end())
    throw std::logic_error("Missing 'example'");
  if (std::find(lst.begin(), lst.end(), "magic") == lst.end())
    throw std::logic_error("Missing 'magic'");
}

static int test_loading(Loader *loader)
{
  // These should fail, for different reasons

  // Test that loading something non-existant works
  try {
    loader->load("test");
    return EXIT_FAILURE;
  }
  catch (bad_plugin& err) {
    string text(err.what());
    if (text.find("test.so: cannot open") == string::npos)
      throw;
  }
  catch (bad_section& err) {
    string text(err.what());
    if (text.find("Section name 'test'") == string::npos)
      throw;
  }

  try {
    loader->load("bad_one");
    return EXIT_FAILURE;
  }
  catch (bad_section& err) {
    string text(err.what());
    if (text.find("Section name 'foobar'") == string::npos)
      throw;
  }

  try {
    loader->load("bad_two");
    return EXIT_FAILURE;
  }
  catch (bad_plugin& err) {
    string text(err.what());

    // This is checking the message, but we should probably define a
    // specialized exception and catch that.
    if (text.find("version was 1.2.3, expected >>1.2.3") == string::npos)
      throw;
  }

  // These should all be OK.
  check_loading(loader, "example", [](const Plugin& plugin){
      expect_equal(plugin.brief, "An example plugin");
    });
  check_loading(loader, "magic", [](const Plugin& plugin){
      expect_equal(plugin.brief, "A magic plugin");
    });

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
  const string prefix(dirname(argv[0]) + "/");
  std::map<string, string> params;
  params["program"] = "harness";
  params["prefix"] = prefix;

  expect_exception<bad_section>([&]{
    Loader loader("harness", prefix + "data/tests-bad-1.cfg", params);
    });

  expect_exception<bad_section>([&]{
    Loader loader("harness", prefix + "data/tests-bad-2.cfg", params);
    });

  {
    Loader loader("harness", prefix + "data/tests.cfg", params);
    test_available(&loader);
    if (int error = test_loading(&loader))
      exit(error);
    test_init(&loader);
  }

  exit(EXIT_SUCCESS);
}

