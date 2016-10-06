/*
  Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLROUTER_XPROTOCOL_DEF_INCLUDED
#define MYSQLROUTER_XPROTOCOL_DEF_INCLUDED

#ifdef _WIN32
#  ifdef X_PROTOCOL_DEFINE_DYNAMIC
#    ifdef X_PROTOCOL_EXPORTS
#      define X_PROTOCOL_API __declspec(dllexport)
#    else
#      define X_PROTOCOL_API __declspec(dllimport)
#    endif
#  else
#    define X_PROTOCOL_API
#  endif
#else
#  define X_PROTOCOL_API
#endif

#endif // MYSQLROUTER_XPROTOCOL_DEF_INCLUDED
