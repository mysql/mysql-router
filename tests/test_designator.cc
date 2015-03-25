#include <mysql/harness/plugin.h>

#include "designator.h"

#include "helpers.h"
#include "utilities.h"

#include <iostream>

void check_desig(const std::string& input,
                 const std::string& plugin)
{
  Designator desig(input);
  expect_equal(desig.plugin.c_str(), plugin.c_str());
}

void check_desig(const std::string& input,
                 const std::string& plugin,
                 Designator::Relation relation,
                 long major_version,
                 long minor_version,
                 long patch_version)
{
  Designator desig(input);
  expect_equal(desig.plugin.c_str(), plugin.c_str());

  expect_equal(static_cast<int>(desig.constraint.size()), 1);
  std::pair<Designator::Relation, Version> elem = desig.constraint.front();
  expect_equal(elem.first, relation);
  expect_equal(elem.second.ver_major, major_version);
  expect_equal(elem.second.ver_minor, minor_version);
  expect_equal(elem.second.ver_patch, patch_version);
}


void check_desig(const std::string& input,
                 const std::string& plugin,
                 Designator::Relation relation1,
                 long major_version1,
                 long minor_version1,
                 long patch_version1,
                 Designator::Relation relation2,
                 long major_version2,
                 long minor_version2,
                 long patch_version2)
{
  Designator desig(input);
  expect_equal(desig.plugin.c_str(), plugin.c_str());

  expect_equal(static_cast<int>(desig.constraint.size()), 2);
  std::pair<Designator::Relation, Version> elem1 = desig.constraint[0];
  expect_equal(elem1.first, relation1);
  expect_equal(elem1.second.ver_major, major_version1);
  expect_equal(elem1.second.ver_minor, minor_version1);
  expect_equal(elem1.second.ver_patch, patch_version1);

  std::pair<Designator::Relation, Version> elem2 = desig.constraint[1];
  expect_equal(elem2.first, relation2);
  expect_equal(elem2.second.ver_major, major_version2);
  expect_equal(elem2.second.ver_minor, minor_version2);
  expect_equal(elem2.second.ver_patch, patch_version2);
}

void test_good_designators()
{
  check_desig("foo", "foo");
  check_desig("foo(<<1)", "foo",
              Designator::LESS_THEN, 1, 0, 0);
  check_desig("foo (<=1.2)  ", "foo",
              Designator::LESS_EQUAL, 1, 2, 0);
  check_desig("foo  (  >>  1.2.3  ) \t",
              "foo", Designator::GREATER_THEN, 1, 2, 3);
  check_desig("foo\t(!=1.2.55)\t",
              "foo", Designator::NOT_EQUAL, 1, 2, 55);
  check_desig("foo\t(==1.4711.001)\t",
              "foo", Designator::EQUAL, 1, 4711, 1);

  check_desig("foo (<=1.2, >>1.3)  ", "foo",
              Designator::LESS_EQUAL, 1, 2, 0,
              Designator::GREATER_THEN, 1, 3, 0);
  check_desig("foo (<=1.2 , >>1.3)  ", "foo",
              Designator::LESS_EQUAL, 1, 2, 0,
              Designator::GREATER_THEN, 1, 3, 0);
  check_desig("foo(<=1.2,>>1.3)", "foo",
              Designator::LESS_EQUAL, 1, 2, 0,
              Designator::GREATER_THEN, 1, 3, 0);
}


void test_bad_designators()
{
  const char *strings[] = {
    "foo(",
    "foo\t(!1.2.55)",
    "foo\t(!1.2.55)",
    "foo\t(=1.2.55)",
    "foo\t(<1.2.55)",
    "foo\t(<<1.2.",
    "foo\t(<<1.2",
    "foo\t(<<.2.55)",
    "foo\t(<<1.2.55",
    "foo<<1.2.55",
  };

  for (auto input: make_range(strings, sizeof(strings)/sizeof(*strings)))
    expect_exception<std::runtime_error>([&input]{ Designator desig(input); });
}

void test_version()
{
  expect(Version(1,0,0) == Version(1,0,0), true);
  expect(Version(1,0,0) < Version(1,0,0), false);
  expect(Version(1,0,0) <= Version(1,0,0), true);
  expect(Version(1,0,0) > Version(1,0,0), false);
  expect(Version(1,0,0) >= Version(1,0,0), true);

  expect(Version(1,0,0) == Version(1,0,1), false);
  expect(Version(1,0,0) < Version(1,0,1), true);
  expect(Version(1,0,0) <= Version(1,0,1), true);
  expect(Version(1,0,0) > Version(1,0,1), false);
  expect(Version(1,0,0) >= Version(1,0,1), false);

  expect(Version(1,0,0) == Version(1,1,0), false);
  expect(Version(1,0,0) < Version(1,1,0), true);
  expect(Version(1,0,0) <= Version(1,1,0), true);
  expect(Version(1,0,0) > Version(1,1,0), false);
  expect(Version(1,0,0) >= Version(1,1,0), false);

  expect(Version(1,0,0) == Version(1,1,5), false);
  expect(Version(1,0,0) < Version(1,1,5), true);
  expect(Version(1,0,0) <= Version(1,1,5), true);
  expect(Version(1,0,0) > Version(1,1,5), false);
  expect(Version(1,0,0) >= Version(1,1,5), false);

  expect(Version(1,0,0) == Version(2,1,5), false);
  expect(Version(1,0,0) < Version(2,1,5), true);
  expect(Version(1,0,0) <= Version(2,1,5), true);
  expect(Version(1,0,0) > Version(2,1,5), false);
  expect(Version(1,0,0) >= Version(2,1,5), false);

  std::cerr << Version(VERSION_NUMBER(1,0,0)) << std::endl;
  expect(Version(VERSION_NUMBER(1,0,0)) == Version(1,0,0), true);
  expect(Version(VERSION_NUMBER(1,1,0)) == Version(1,1,0), true);
  expect(Version(VERSION_NUMBER(1,2,0)) == Version(1,2,0), true);
  expect(Version(VERSION_NUMBER(1,0,2)) == Version(1,0,2), true);
  expect(Version(VERSION_NUMBER(1,2,3)) == Version(1,2,3), true);
}

void check_constraint(const std::string& str, const Version& ver, bool expect)
{
  Designator designator(str);
  expect(designator.version_good(ver), expect);
}

void test_constraints()
{
  check_constraint("foo(<< 1.2)", Version(1,1), true);
  check_constraint("foo(<< 1.2)", Version(1,2), false);
  check_constraint("foo(<= 1.2)", Version(1,2), true);
  check_constraint("foo(<= 1.2)", Version(1,2,1), false);
  check_constraint("foo(>= 1.2)", Version(1,2,2), true);
  check_constraint("foo(>>1.2)", Version(1,2,2), true);
  check_constraint("foo(>= 1.2, !=1.2.2)", Version(1,2,2), false);
  check_constraint("foo(>> 1.2, !=1.2.2)", Version(1,2,2), false);
  check_constraint("foo(>> 1.2, !=1.2.2)", Version(1,2,3), true);
}


int main()
{
  try {
    test_version();
    test_good_designators();
    test_bad_designators();
    test_constraints();
  }
  catch (std::runtime_error& exc) {
    std::cerr << exc.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
