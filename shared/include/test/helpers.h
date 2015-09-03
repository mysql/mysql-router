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

// Third-party headers
#include "gtest/gtest.h"

// Standard headers
#include <algorithm>
#include <string>
#include <vector>
#include <list>

// Forward declarations
class Loader;

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& v) {
  out << "{";
  for (auto&& elem: v)
    out << " " << elem;
  out << " }";
  return out;
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::list<T>& v) {
  out << "{";
  for (auto&& elem: v)
    out << " " << elem;
  out << " }";
  return out;
}

template <typename A, typename B>
std::ostream& operator<<(std::ostream& out, const std::pair<A,B>& p) {
  return out << p.first << ":" << p.second;
}

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
                      std::back_inserter(c1_not_c2));
  if (c1_not_c2.size() > 0) {
    auto result = ::testing::AssertionFailure();
    result << seq1_expr << " had elements not in " << seq2_expr << ": ";
    for (auto elem: c1_not_c2)
      result << elem << " ";
    return result;
  }

  // Check for elements that are in the second range but not in the first.
  std::vector<typename SeqCont2::value_type> c2_not_c1;
  std::set_difference(c2.begin(), c2.end(), c1.begin(), c1.end(),
                      std::back_inserter(c2_not_c1));
  if (c2_not_c1.size() > 0) {
    auto result = ::testing::AssertionFailure();
    result << seq2_expr << " had elements not in " << seq1_expr << ": ";
    for (auto elem: c2_not_c1)
      result << elem << " ";
    return result;
  }

  return ::testing::AssertionSuccess();
}

#define EXPECT_SETEQ(S1, S2) \
  EXPECT_PRED_FORMAT2(AssertSetEqual, S1, S2)

::testing::AssertionResult
AssertLoaderSectionAvailable(const char *loader_expr,
                             const char *section_expr,
                             Loader* loader,
                             const std::string& section_name);

#define EXPECT_SECTION_AVAILABLE(S, L)  \
  EXPECT_PRED_FORMAT2(AssertLoaderSectionAvailable, L, S)

#endif /* HELPERS_INCLUDED */
