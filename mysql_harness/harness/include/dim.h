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

#ifndef MYSQL_HARNESS_DIMANAGER_INCLUDED
#define MYSQL_HARNESS_DIMANAGER_INCLUDED

#include "harness_export.h"
#include "unique_ptr.h"

#include <functional>
#include <string>   // unfortunately, std::string is a typedef and therefore not easy to forward-declare



/** @class DIM (Dependency Injection Manager)
 *  @brief Provides simple, yet useful dependency injection mechanism
 *
 * Adding a new managed object is simple:
 *   Step 1: add class forward declaration
 *   Step 2: add object factory + deleter setter
 *   Step 3: add singleton object getter or object creator. Adding both usually makes no sense
 *   Step 4: add factory and deleter function objects
 *
 * Then, you also need to call the factory setter somewhere in your main program,
 * before you use the object getter.
 *
 *
 *
 * USAGE EXAMPLE (hint: copy one set of members+methods and modify for your object)
 *
 *   // forward declarations [step 1]
 *   class Foo;
 *   class Bar;
 *   class Baz;
 *
 *   class DIM {
 *     ... constructors, instance(), other support methods ...
 *
 *     // Example: Foo depends on Bar and Baz,
 *     //          Bar depends on Baz and some int,
 *     //          Baz depends on nothing
 *
 *    public:
 *     // factory + deleter setters [step 2]
 *     void set_Foo(const std::function<Foo*(void)>& factory, const std::function<void(Foo*)>& deleter = std::default_delete<Foo>()) { factory_Foo_ = factory; deleter_Foo_ = deleter; }
 *     void set_Bar(const std::function<Bar*(void)>& factory, const std::function<void(Bar*)>& deleter = std::default_delete<Bar>()) { factory_Bar_ = factory; deleter_Bar_ = deleter; }
 *     void set_Baz(const std::function<Baz*(void)>& factory, const std::function<void(Baz*)>& deleter = std::default_delete<Baz>()) { factory_Baz_ = factory; deleter_Baz_ = deleter; }
 *
 *     // singleton object getters (all are shown, but normally mutually-exclusive with next group) [step 3]
 *     Foo& get_Foo() const { return get_generic<Foo>(factory_Foo_, deleter_Foo_); }
 *     Bar& get_Bar() const { return get_generic<Bar>(factory_Bar_, deleter_Bar_); }
 *     Baz& get_Baz() const { return get_generic<Baz>(factory_Baz_, deleter_Baz_); }
 *
 *     // object creators (all are shown, but normally mutually-exclusive with previous group) [step 3]
 *     UniquePtr<Foo> new_Foo() const { return new_generic(factory_Foo_, deleter_Foo_); }
 *     UniquePtr<Bar> new_Bar() const { return new_generic(factory_Bar_, deleter_Bar_); }
 *     UniquePtr<Baz> new_Baz() const { return new_generic(factory_Baz_, deleter_Baz_); }
 *
 *    private:
 *     // factory and deleter function objects [step 4]
 *     std::function<Foo*(void)> factory_Foo_;  std::function<void(Foo*)> deleter_Foo_;
 *     std::function<Bar*(void)> factory_Bar_;  std::function<void(Bar*)> deleter_Bar_;
 *     std::function<Baz*(void)> factory_Baz_;  std::function<void(Baz*)> deleter_Baz_;
 *   };
 *
 *
 *
 *   // actual classes
 *   struct Baz {
 *     Baz() {}
 *   };
 *   struct Bar {
 *     Bar(Baz, int) {}
 *   };
 *   struct Foo {
 *     Foo(Bar, Baz) {}
 *     void do_something() {}
 *   };
 *
 *
 *
 *   // usage
 *   int main() {
 *     int n = 3306;
 *
 *     // init code
 *     DIM& dim = DIM::instance();
 *     dim.set_Foo([&dim]()    { return new Foo(dim.get_Bar(), dim.get_Baz()); });
 *     dim.set_Bar([&dim, n]() { return new Bar(dim.get_Baz(), n);             });
 *     dim.set_Baz([&dim]()    { return new Baz;                               });
 *
 *     // use code (as singleton)
 *     dim.get_Foo().do_something(); // will automatically instantiate Bar and Baz as well
 *
 *     // use code (as new object)
 *     UniquePtr<Foo> foo = dim.new_Foo();
 *     foo->do_something();
 *   }
 */

// forward declarations [step 1]
namespace mysqlrouter { class MySQLSession; }
namespace mysqlrouter { class Ofstream; }

namespace mysql_harness {

class HARNESS_EXPORT DIM { // DIM = Dependency Injection Manager

  // this class is a singleton
  protected:
  DIM();
  ~DIM();
  public:
  DIM(const DIM&) = delete;
  DIM& operator==(const DIM&) = delete;
  static DIM& instance();

  // NOTE: once we gain confidence in this DIM and we can treat it as black box,
  //       all the boilerplate stuff (steps 2-4) for each class can be generated by a macro)

 public:
  ////////////////////////////////////////////////////////////////////////////////
  // factory and deleter setters [step 2]
  ////////////////////////////////////////////////////////////////////////////////

  // MySQLSession
  void set_MySQLSession(const std::function<mysqlrouter::MySQLSession*(void)>& factory,
                        const std::function<void(mysqlrouter::MySQLSession*)>& deleter
                              = std::default_delete<mysqlrouter::MySQLSession>()) {
    factory_MySQLSession_ = factory;
    deleter_MySQLSession_ = deleter;
  }

  // Ofstream
  void set_Ofstream(const std::function<mysqlrouter::Ofstream*(void)>& factory,
                    const std::function<void(mysqlrouter::Ofstream*)>& deleter
                          = std::default_delete<mysqlrouter::Ofstream>()) {
    factory_Ofstream_ = factory;
    deleter_Ofstream_ = deleter;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // object getters [step 3]
  ////////////////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////////////////
  // object creators [step 3]
  ////////////////////////////////////////////////////////////////////////////////

  // MySQLSession
  UniquePtr<mysqlrouter::MySQLSession> new_MySQLSession() const { return new_generic(factory_MySQLSession_, deleter_MySQLSession_); }

  // Ofstream
  UniquePtr<mysqlrouter::Ofstream> new_Ofstream() const { return new_generic(factory_Ofstream_, deleter_Ofstream_); }

 private:
  ////////////////////////////////////////////////////////////////////////////////
  // factory and deleter functions [step 4]
  ////////////////////////////////////////////////////////////////////////////////

  // MySQLSession
  std::function<mysqlrouter::MySQLSession*(void)> factory_MySQLSession_;
  std::function<void(mysqlrouter::MySQLSession*)> deleter_MySQLSession_;

  // Ofstream
  std::function<mysqlrouter::Ofstream*(void)> factory_Ofstream_;
  std::function<void(mysqlrouter::Ofstream*)> deleter_Ofstream_;





  ////////////////////////////////////////////////////////////////////////////////
  // utility functions
  ////////////////////////////////////////////////////////////////////////////////

 protected:
  // get_generic*() (add more variants if needed, or convert into varargs template)
  template <typename T>
  static T& get_generic(const std::function<T*(void)>& factory, const std::function<void(T*)>& deleter) {
    static UniquePtr<T> obj = new_generic(factory, deleter);
    return *obj;
  }
  template <typename T, typename A1>
  static T& get_generic1(const std::function<T*(A1)>& factory, const std::function<void(T*)>& deleter, const A1& a1) {
    static UniquePtr<T> obj = new_generic1(factory, deleter, a1);
    return *obj;
  }
  template <typename T, typename A1, typename A2>
  static T& get_generic1(const std::function<T*(A1, A2)>& factory, const std::function<void(T*)>& deleter, const A1& a1, const A2& a2) {
    static UniquePtr<T> obj = new_generic1(factory, deleter, a1, a2);
    return *obj;
  }

  // new_generic*() (add more variants if needed, or convert into varargs template)
  template <typename T>
  static UniquePtr<T> new_generic(const std::function<T*(void)>& factory, const std::function<void(T*)>& deleter) {
    return UniquePtr<T>(
      factory(),
      [deleter](T* p){ deleter(p); }  // [&deleter] would be unsafe if set_T() was called before this object got erased
    );
  }
  template <typename T, typename A1>
  static UniquePtr<T> new_generic1(const std::function<T*(A1)>& factory, const std::function<void(T*)>& deleter, const A1& a1) {
    return UniquePtr<T>(
      factory(a1),
      [deleter](T* p){ deleter(p); }  // [&deleter] would be unsafe if set_T() was called before this object got erased
    );
  }
  template <typename T, typename A1, typename A2>
  static UniquePtr<T> new_generic2(const std::function<T*(A1,A2)>& factory, const std::function<void(T*)>& deleter, const A1& a1, const A2& a2) {
    return UniquePtr<T>(
      factory(a1, a2),
      [deleter](T* p){ deleter(p); }  // [&deleter] would be unsafe if set_T() was called before this object got erased
    );
  }

};  // class DIM

} // namespace mysql_harness
#endif //#ifndef MYSQL_HARNESS_DIMANAGER_INCLUDED
