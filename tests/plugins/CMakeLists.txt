# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

add_plugin(bad_one NO_INSTALL
  DESTINATION_PREFIX harness
  SOURCES bad_one.cc)
add_plugin(bad_two NO_INSTALL
  DESTINATION_PREFIX harness
  SOURCES bad_two.cc)
add_plugin(magic NO_INSTALL
  DESTINATION_PREFIX harness
  INTERFACE include
  SOURCES magic.cc)
add_plugin(example NO_INSTALL
  DESTINATION_PREFIX harness
  SOURCES example.cc
  REQUIRES magic logger)

add_custom_command(TARGET example POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:example> ../var/lib/harness/$<TARGET_FILE_NAME:example>)
add_custom_command(TARGET example2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:example2> ../var/lib/harness/$<TARGET_FILE_NAME:example2>)
add_custom_command(TARGET bad_one POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bad_one> ../var/lib/harness/$<TARGET_FILE_NAME:bad_one>)
add_custom_command(TARGET bad_two POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bad_two> ../var/lib/harness/$<TARGET_FILE_NAME:bad_two>)
add_custom_command(TARGET magic POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:magic> ../var/lib/harness/$<TARGET_FILE_NAME:magic>)
