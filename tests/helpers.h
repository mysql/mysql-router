#ifndef HELPERS_INCLUDED
#define HELPERS_INCLUDED

#include <stdexcept>
#include <string>
#include <typeinfo>
#include <cstring>

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

template <class Type> struct CompareTraits;

template <>
struct CompareTraits<const char*>
{
  static bool equal(const char *a, const char *b) {
    return strcmp(a, b) == 0;
  }

  static const char *c_str(const char* val) {
    return val;
  }
};

template <>
struct CompareTraits<long>
{
  static bool equal(long a, long b) {
    return a == b;
  }

  static bool less(long a, long b) {
    return a < b;
  }

  static const char *c_str(long val) {
    static char buf[16];
    sprintf(buf, "%ld", val);
    return buf;
  }
};

template <class Type, class Traits = CompareTraits<Type> >
void expect_equal(Type value, Type expect, Traits traits = Traits())
{
  if (!traits.equal(value, expect))
  {
    char buf[256];
    sprintf(buf, "Expected %s, got %s",
            traits.c_str(expect),
            traits.c_str(value));
    throw std::runtime_error(buf);
  }
}

template <class Type, class Traits = CompareTraits<Type> >
void expect_less(Type value, Type expect, Traits traits = Traits())
{
  if (!traits.less(value, expect))
  {
    char buf[256];
    sprintf(buf, "Expected something less than %s, got %s",
            traits.c_str(expect),
            traits.c_str(value));
    throw std::runtime_error(buf);
  }
}

#endif /* HELPERS_INCLUDED */
