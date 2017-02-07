/*
  Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "gtest/gtest.h"

#include "dim.h"
#include "random_generator.h"

#include <map>



TEST(UtilsTests, generate_password) {
  // here we test that generate_password():
  // - picks alphanums from the spectrum requested (if we want 10, it should choose randoms
  //   between '0' and '9', if we want 2, should choose between '0' and '1', etc)
  // - returns the right number of them

  // number large enough so that (in practice) at least one representative of each
  // possible random char will be present in the output.  Obviously nothing is 100%
  // guaranteed, the idea is to make random test failures very very very unlikely.
  constexpr unsigned kBigNumber = 10 * 1000;

  constexpr unsigned kMaxBase = 87;  // this is the max base (atm)

  mysql_harness::RandomGenerator rg;

  // min random base
  {
    std::string s = rg.generate_password(kBigNumber, 2);
    std::map<char, unsigned> hist;
    for (char c : s)
      hist[c]++;
    EXPECT_EQ(2u, hist.size());  // if this failed, you've won the jackpot! (please rerun)
    EXPECT_TRUE(hist.count('0'));
    EXPECT_TRUE(hist.count('1'));
    EXPECT_EQ(kBigNumber, hist['0'] + hist['1']);
  }

  // max random base (supported atm)
  {
    std::string s = rg.generate_password(kBigNumber, kMaxBase);
    std::map<char, unsigned> hist;
    for (char c : s)
      hist[c]++;
    EXPECT_EQ(kMaxBase, hist.size()); // if this failed, you've won the jackpot! (please rerun)

    unsigned total_chars = 0;
    for (const auto& i : hist)
      total_chars += i.second;
    EXPECT_EQ(kBigNumber, total_chars);
  }

  // max random base (supported atm) - implicit base
  {
    std::string s = rg.generate_password(kBigNumber);
    std::map<char, unsigned> hist;
    for (char c : s)
      hist[c]++;
    EXPECT_EQ(kMaxBase, hist.size()); // if this failed, you've won the jackpot! (please rerun)

    unsigned total_chars = 0;
    for (const auto& i : hist)
      total_chars += i.second;
    EXPECT_EQ(kBigNumber, total_chars);
  }

  // random base 10
  {
    std::string s = rg.generate_password(kBigNumber, 10);
    std::map<char, unsigned> hist;
    for (char c : s)
      hist[c]++;
    EXPECT_EQ(10u, hist.size()); // if this failed, you've won the jackpot! (please rerun)

    unsigned total_chars = 0;
    for (char c = '0'; c <= '9'; c++) {
      EXPECT_NE(0u, hist[c]);
      total_chars += hist[c];
    }
    EXPECT_EQ(kBigNumber, total_chars);
  }

  // random base 36
  {
    std::string s = rg.generate_password(kBigNumber, 36);
    std::map<char, unsigned> hist;
    for (char c : s)
      hist[c]++;
    EXPECT_EQ(36u, hist.size()); // if this failed, you've won the jackpot! (please rerun)

    unsigned total_chars = 0;
    for (char c = '0'; c <= '9'; c++) {
      EXPECT_NE(0u, hist[c]);
      total_chars += hist[c];
    }
    for (char c = 'a'; c <= 'z'; c++) {
      EXPECT_NE(0u, hist[c]);
      total_chars += hist[c];
    }
    EXPECT_EQ(kBigNumber, total_chars);
  }

  // length = 0
  {
    std::string s = rg.generate_password(0, 10);
    EXPECT_EQ(0u, s.size());
  }

  // length = 1
  {
    std::string s = rg.generate_password(1, 10);
    EXPECT_EQ(1u, s.size());
  }
}

