include_directories(${CMAKE_SOURCE_DIR}/harness)

add_subdirectory(plugins)

macro(MY_ADD_TEST name)
  get_filename_component(test_name ${ARGV1} NAME_WE)
  add_executable(${test_name} ${ARGN})
  target_link_libraries(${test_name} harness-archive)
  add_test(${name} ${test_name})
endmacro()

my_add_test(TestLoader test_loader.cc)
my_add_test(TestIterator test_iterator.cc)
my_add_test(TestUtilities test_utilities.cc)
my_add_test(TestConfig test_config.cc)
  