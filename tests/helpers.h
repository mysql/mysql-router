/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef HELPERS_INCLUDED
#define HELPERS_INCLUDED

#include <algorithm>
#include <string>
#include <vector>

#include "gtest/gtest.h"

template <typename SeqCont1, typename SeqCont2>
::testing::AssertionResult
AssertSetEqual(const char* seq1_expr, const char *seq2_expr,
               const SeqCont1& seq1, const SeqCont2& seq2)
{
  std::vector<typename SeqCont1::value_type> c1(seq1.begin(), seq1.end());
  std::vector<typename SeqCont2::value_type> c2(seq2.begin(), seq2.end());
  std::sort(c1.begin(), c1.end());
  std::sort(c2.begin(), c2.end());

  // Check for elements that are in the first range but not in the second.
  std::vector<typename SeqCont2::value_type> c1_not_c2;
  std::set_difference(c1.begin(), c1.end(), c2.begin(), c2.end(),
                      std::inserter(c1_not_c2, c1_not_c2.begin()));
  if (c1_not_c2.size() > 0) {
    auto result = ::testing::AssertionFailure()
      << seq1_expr << " had elements not in " << seq2_expr << ": ";
    for (auto elem: c1_not_c2)
      result << elem << " ";
    return result;
  }

  // Check for elements that are in the second range but not in the first.
  std::vector<typename SeqCont2::value_type> c2_not_c1;
  std::set_difference(c2.begin(), c2.end(), c1.begin(), c1.end(),
                      std::inserter(c2_not_c1, c2_not_c1.begin()));
  if (c2_not_c1.size() > 0) {
    auto result = ::testing::AssertionFailure()
      << seq2_expr << " had elements not in " << seq1_expr << ": ";
    for (auto elem: c2_not_c1)
      result << elem << " ";
    return result;
  }

  return ::testing::AssertionSuccess();
}

#define EXPECT_SETEQ(S1, S2) \
  EXPECT_PRED_FORMAT2(AssertSetEqual, S1, S2)

#endif /* HELPERS_INCLUDED */
