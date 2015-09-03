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

#ifndef UTILITIES_INCLUDED
#define UTILITIES_INCLUDED

#include <string>

/**
 * Class to turn C array into range.
 *
 * @see make_range
 */

template <class Type>
class Range {
public:
  class iterator
  {
  public:
    iterator(Type* ptr) : m_ptr(ptr)  {}

    iterator& operator++() {
      ++m_ptr;
      return *this;
    }

    bool operator==(const iterator& rhs) {
      return m_ptr == rhs.m_ptr;
    }

    bool operator!=(const iterator& rhs) {
      return m_ptr != rhs.m_ptr;
    }

    Type& operator*() {
      return *m_ptr;
    }

    const Type& operator*() const {
      return *m_ptr;
    }

  private:
    Type *m_ptr;
  };

  Range(Type* ptr, size_t length)
    : m_start(ptr), m_finish(ptr + length)
  {
  }

  iterator begin() {
    return iterator(m_start);
  }

  iterator end() {
    return iterator(m_finish);
  }

private:
  Type* m_start;
  Type* m_finish;
};


/**
 * Create a range from a plain C array.
 *
 * This function create a range from a plain array so that arrays can
 * be used in range-based loops.
 *
 * @see Range
 */

template <class Type>
Range<Type> make_range(Type* ptr, size_t length)
{
  return Range<Type>(ptr, length);
}


/**
 * Class for creating a reverse range from another range.
 */

template <typename Range>
class RangeReverse
{
public:

  RangeReverse(Range& range)
    : m_range(range)
  {
  }

  typename Range::reverse_iterator begin()
  {
    return m_range.rbegin();
  }

  typename Range::const_reverse_iterator begin() const
  {
    return m_range.rbegin();
  }

  typename Range::reverse_iterator end()
  {
    return m_range.rend();
  }

  typename Range::const_reverse_iterator end() const
  {
    return m_range.rend();
  }

private:
  Range& m_range;
};


/**
 * Iterate over a range in reverse.
 *
 * Function take a range, which can be any sequence container, and
 * return a reverse range that iterate the sequence in reverse.
 *
 * Typical use-case is:
 * @code
 * for (auto item : reverse_iterate(my_list)) {
 *   ...
 * }
 * @endcode
 */
template <typename Range>
RangeReverse<Range> reverse(Range& x)
{
  return RangeReverse<Range>(x);
}

template <class Map>
std::pair<typename Map::iterator, typename Map::iterator>
find_range_first(Map& assoc,
                 const typename Map::key_type::first_type& first,
                 typename Map::iterator start)
{
  typename Map::iterator finish = start;
  while (finish != assoc.end() && finish->first.first == first)
    ++finish;
  return make_pair(start, finish);
}

template <class Map>
std::pair<typename Map::iterator, typename Map::iterator>
find_range_first(Map& assoc,
                 const typename Map::key_type::first_type& first)
{
  typedef typename Map::key_type::second_type SType;
  return find_range_first(assoc, first,
                          assoc.lower_bound(make_pair(first, SType())));
}


template <class Map>
std::pair<typename Map::const_iterator, typename Map::const_iterator>
find_range_first(const Map& assoc,
                 const typename Map::key_type::first_type& first,
                 typename Map::const_iterator start)
{
  typename Map::const_iterator finish = start;
  while (finish != assoc.end() && finish->first.first == first)
    ++finish;
  return make_pair(start, finish);
}

template <class Map>
std::pair<typename Map::const_iterator, typename Map::const_iterator>
find_range_first(const Map& assoc,
                 const typename Map::key_type::first_type& first)
{
  typedef typename Map::key_type::second_type SType;
  return find_range_first(assoc, first,
                          assoc.lower_bound(make_pair(first, SType())));
}

void strip(std::string& str, const char* chars = " \t\n\r\f\v");

#endif /* UTILITIES_INCLUDED */
