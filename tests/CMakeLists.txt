include_directories(${PROJECT_SOURCE_DIR}/harness)
include_directories(${CMAKE_BINARY_DIR}/include)

add_subdirectory(plugins)

macro(MY_ADD_TEST name)
  set(doing)
  set(sources)
  set(requires)
  foreach(arg ${ARGN})
    if(arg MATCHES "^(SOURCES|REQUIRES)$")
      set(doing ${arg})
    elseif(doing MATCHES "^SOURCES$")
      list(APPEND sources ${arg})
    elseif(doing MATCHES "^REQUIRES")
      list(APPEND requires "${arg}-INTERFACE")
    else()
      message(AUTHOR_WARNING "Unknown argument: '${arg}'")
    endif()
  endforeach()

  add_executable(${name} ${sources})
  if(requires)
    add_dependencies(${name} ${requires})
  endif()
  target_link_libraries(${name} harness-archive)
  add_test(${name} ${name})
endmacro()

my_add_test(TestLoader SOURCES test_loader.cc REQUIRES harness)
add_custom_command(TARGET TestLoader POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:logger> ${CMAKE_CURRENT_BINARY_DIR}/var/lib/${HARNESS_NAME}/$<TARGET_FILE_NAME:logger>)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/lib
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/log
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/lib/${HARNESS_NAME}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/var/log/${HARNESS_NAME}
)

my_add_test(TestDesignator SOURCES test_designator.cc)
my_add_test(TestIterator SOURCES test_iterator.cc)
my_add_test(TestUtilities SOURCES test_utilities.cc)

my_add_test(TestFilesystem SOURCES test_filesystem.cc REQUIRES harness)
my_add_test(TestConfig SOURCES test_config.cc REQUIRES harness)

add_custom_command(TARGET TestConfig POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/tests/data ${CMAKE_BINARY_DIR}/tests/data)
