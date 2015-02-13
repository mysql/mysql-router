add_plugin(example example.cc NO_INSTALL)
add_plugin(magic magic.cc NO_INSTALL)

add_custom_command(TARGET example POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:example> ../var/lib/${HARNESS_NAME}/$<TARGET_FILE_NAME:example>)
add_custom_command(TARGET magic POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:magic> ../var/lib/${HARNESS_NAME}/$<TARGET_FILE_NAME:magic>)
