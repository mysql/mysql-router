add_plugin(bad_one NO_INSTALL SOURCES bad_one.cc)
add_plugin(bad_two NO_INSTALL SOURCES bad_two.cc)
add_plugin(magic NO_INSTALL SOURCES magic.cc)
add_plugin(example NO_INSTALL
           SOURCES example.cc
           REQUIRES magic logger)

add_custom_command(TARGET example POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:example> ../var/lib/${HARNESS_NAME}/$<TARGET_FILE_NAME:example>)
add_custom_command(TARGET bad_one POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bad_one> ../var/lib/${HARNESS_NAME}/$<TARGET_FILE_NAME:bad_one>)
add_custom_command(TARGET bad_two POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bad_two> ../var/lib/${HARNESS_NAME}/$<TARGET_FILE_NAME:bad_two>)
add_custom_command(TARGET magic POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:magic> ../var/lib/${HARNESS_NAME}/$<TARGET_FILE_NAME:magic>)
