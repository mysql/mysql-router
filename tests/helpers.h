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
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <iterator>
#include <vector>


template <class Exception, class Function>
void expect_exception(Function func)
{
  try
  {
    func();
  }
  catch (Exception& exc)
  {
    return;
  }

  std::string message("Expected exception ");
  message += typeid(Exception).name();

  throw std::runtime_error(message);
}

template <class Type>
struct TestTraits
{
  bool equal(Type a, Type b) { return a == b; }
  bool less(Type a, Type b) { return a < b; }
  void show_not_equal(std::ostream& out, const Type& value, const Type& expect)
  {
    out << "Expected " << expect << ", got " << value;
  }
};

template <class Elem>
struct TestTraits<std::vector<Elem>>
{
  bool equal(const std::vector<Elem>& lhs, const std::vector<Elem>& rhs)
  {
    if (lhs.size() != rhs.size())
        return false;

    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
  }

  void show_not_equal(std::ostream& out,
                      const std::vector<Elem>& value,
                      const std::vector<Elem>& expect)
  {
    copy(value.begin(), value.end(),
         typename std::ostream_iterator<Elem>(out, " "));
    out << " and ";
    copy(expect.begin(), expect.end(),
         typename std::ostream_iterator<Elem>(out, " "));
    out << " not equal";
  }
};

template <>
struct TestTraits<const char*>
{
  bool equal(const char *a, const char *b)
  {
    return strcmp(a, b) == 0;
  }

  void show_not_equal(std::ostream& out, const char* value, const char* expect)
  {
    out << "Expected " << expect << ", got " << value;
  }
};

void _expect(bool value, const std::string& expr, const std::string& expect)
{
  if (!value)
    throw std::runtime_error("Expected expression " + std::string(expr) +
                             " to be " + expect);
}

#define expect(EXPR, BOOL) _expect((EXPR) == (BOOL), #EXPR, #BOOL)

template <class Type1, class Type2, class Traits = TestTraits<Type1>>
void expect_equal(Type1 value, Type2 expect, Traits traits = Traits())
{
  if (!traits.equal(value, expect))
  {
    std::ostringstream buffer;
    traits.show_not_equal(buffer, value, expect);
    throw std::runtime_error(buffer.str());
  }
}

template <class Type, class Traits = TestTraits<Type>>
void expect_less(Type value, Type expect, Traits traits = Traits())
{
  if (!traits.less(value, expect))
  {
    std::ostringstream buffer;
    buffer << "Expected something less than " << expect << ", got " << value;
    throw std::runtime_error(buffer.str());
  }
}

#endif /* HELPERS_INCLUDED */
