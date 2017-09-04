/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_COMPILER_ATTRIBUTES_INCLUDED
#define MYSQL_HARNESS_COMPILER_ATTRIBUTES_INCLUDED

#if defined(__GNUC__)
#  define ATTRIBUTE_GCC_FORMAT(style, fmt_pos, arg_pos) __attribute__((format(style, fmt_pos, arg_pos)))
#else
#  define ATTRIBUTE_GCC_FORMAT(style, fmt_pos, arg_pos)
#endif

#endif // MYSQL_HARNESS_COMPILER_ATTRIBUTES_INCLUDED
