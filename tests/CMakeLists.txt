include_directories(${CMAKE_SOURCE_DIR}/harness)

add_subdirectory(plugins)

macro(MY_ADD_TEST name)
  get_filename_component(test_name ${ARGV1} NAME_WE)
  add_executable(${test_name} ${ARGN})
  target_link_libraries(${test_name} harness-archive)
  add_test(${name} ${test_name})
endmacro()

my_add_test(TestLoader test_loader.cc)
add_dependencies(test_loader harness-INTERFACE)
add_custom_command(TARGET test_loader POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:logger> ${CMAKE_CURRENT_BINARY_DIR}/var/lib/${HARNESS_NAME}/$<TARGET_FILE_NAME:logger>)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/lib
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/log
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/lib/${HARNESS_NAME}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/log/${HARNESS_NAME}
)

my_add_test(TestDesignator test_designator.cc)
my_add_test(TestIterator test_iterator.cc)
my_add_test(TestUtilities test_utilities.cc)
my_add_test(TestConfig test_config.cc)
add_dependencies(test_config config-INTERFACE)
