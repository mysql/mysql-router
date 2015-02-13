include_directories(${CMAKE_SOURCE_DIR}/harness)

add_subdirectory(plugins)

macro(my_add_test name)
  add_executable(test_loader test_loader.cc)
  target_link_libraries(test_loader harness)
  add_test(${name} test_loader)
endmacro()

add_executable(test_loader test_loader.cc)
target_link_libraries(test_loader harness)
set_target_properties(test_loader PROPERTIES
  COMPILE_DEFINITIONS "HARNESS_NAME=\"${HARNESS_NAME}\"")
add_test(TestLoader test_loader)

add_executable(test_iterator test_iterator.cc)
target_link_libraries(test_loader harness)
add_test(TestIterator test_iterator)

add_executable(test_utilities test_utilities.cc)
target_link_libraries(test_utilities harness)
add_test(TestUtilities test_utilities)
  