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

#ifndef MYSQL_HARNESS_HARNESS_ASSERT_INCLUDED
#define MYSQL_HARNESS_HARNESS_ASSERT_INCLUDED

#include <stdlib.h>

/** Improved assert()
 *
 * This macro is meant to provide analogous functionality to the well-known
 * assert() macro. In contrast to the original, it should also work in
 * release builds.
 */
#define harness_assert(COND) if (!(COND)) abort();

/** assert(0) idiom with an explicit name
 * 
 * This is essentially the assert(0) idiom, but with more explicit name
 * to clarify the intent.
 */
#define harness_assert_this_should_not_execute() \
    harness_assert("If execution reached this line, you have a bug" == nullptr);

#endif /* MYSQL_HARNESS_HARNESS_ASSERT_INCLUDED */

