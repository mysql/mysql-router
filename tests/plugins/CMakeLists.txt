add_plugin(example example.cc NO_INSTALL)
add_plugin(bad_one bad_one.cc NO_INSTALL)
add_plugin(bad_two bad_two.cc NO_INSTALL)
add_plugin(magic magic.cc NO_INSTALL)

add_custom_command(TARGET example POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:example> ../var/lib/${HARNESS_NAME}/$<TARGET_FILE_NAME:example>)
add_custom_command(TARGET bad_one POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bad_one> ../var/lib/${HARNESS_NAME}/$<TARGET_FILE_NAME:bad_one>)
add_custom_command(TARGET bad_two POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bad_two> ../var/lib/${HARNESS_NAME}/$<TARGET_FILE_NAME:bad_two>)
add_custom_command(TARGET magic POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:magic> ../var/lib/${HARNESS_NAME}/$<TARGET_FILE_NAME:magic>)
