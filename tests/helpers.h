#ifndef HELPERS_INCLUDED
#define HELPERS_INCLUDED

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>

template <class Exception, class Function>
void expect_exception(Function func)
{
  try {
    func();
  }
  catch (Exception& exc) {
    return;
  }

  std::string message("Expected exception ");
  message += typeid(Exception).name();

  throw std::runtime_error(message);
}

template <class Type>
struct CompareTraits
{
  static bool equal(Type a, Type b) { return a == b; }
  static bool less(Type a, Type b) { return a < b; }
};

template <>
struct CompareTraits<const char*>
{
  static bool equal(const char *a, const char *b) {
    return strcmp(a, b) == 0;
  }
};

template <>
struct CompareTraits<long>
{
  static bool equal(long a, long b) { return a == b; }
  static bool less(long a, long b) { return a < b; }
};

void _expect(bool value, const std::string& expr, const std::string& expect)
{
  if (!value)
    throw std::runtime_error("Expected expression " + std::string(expr) +
                             " to be " + expect);
}

#define expect(EXPR, BOOL) _expect((EXPR) == (BOOL), #EXPR, #BOOL)

template < class Type1, class Type2, class Traits = CompareTraits<Type1> >
void expect_equal(Type1 value, Type2 expect, Traits traits = Traits())
{
  if (!traits.equal(value, expect))
  {
    std::ostringstream buffer;
    buffer << "Expected " << expect << ", got " << value;
    throw std::runtime_error(buffer.str());
  }
}

template <class Type, class Traits = CompareTraits<Type> >
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
