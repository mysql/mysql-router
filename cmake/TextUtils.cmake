# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

function(oxford_comma _var)
  if(ARGC EQUAL 2)
    set(${_var} "${ARGV1}" PARENT_SCOPE)
  elseif(ARGC EQUAL 3)
    set(${_var} "${ARGV1} and ${ARGV2}" PARENT_SCOPE)
  else()
    set(_count 3)
    set(_glue)
    set(_result)
    foreach(_arg ${ARGN})
      set(_result "${_result}${_glue}${_arg}")
      if(_count LESS ARGC)
        set(_glue ", ")
      else()
        set(_glue ", and ")
      endif()
      math(EXPR _count "${_count}+1")
    endforeach()
    set(${_var} "${_result}" PARENT_SCOPE)
  endif()
endfunction()
