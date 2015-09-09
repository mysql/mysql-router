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

#ifndef DESIGNATOR_INCLUDED
#define DESIGNATOR_INCLUDED

#include <string>
#include <vector>
#include <iostream>
#include <sstream>

/**
 * Class representing a version.
 *
 * Versions consist of a three-position dotted pair
 * `MAJOR.MINOR.PATCH` where MAJOR is the major version number, MINOR
 * is the minor version number, and PATCH is the patch number. Version
 * comparison is done lexicographically in the normal manner so that
 * 1.1.5 < 1.2.1 < 1.2.3.
 */
struct Version {
  friend std::ostream& operator<<(std::ostream& out, const Version& ver) {
    out << ver.str();
    return out;
  }

  friend bool operator<(const Version& lhs, const Version& rhs) {
    return (lhs.ver_major < rhs.ver_major) ||
      (lhs.ver_major == rhs.ver_major && lhs.ver_minor < rhs.ver_minor) ||
      (lhs.ver_minor == rhs.ver_minor && lhs.ver_patch < rhs.ver_patch);
  }

  friend bool operator==(const Version& lhs, const Version& rhs) {
    return (lhs.ver_major == rhs.ver_major) &&
      (lhs.ver_minor == rhs.ver_minor) &&
      (lhs.ver_patch == rhs.ver_patch);
  }

  friend bool operator!=(const Version& lhs, const Version& rhs) {
    return !(lhs == rhs);
  }

  friend bool operator<=(const Version& lhs, const Version& rhs) {
    return (lhs < rhs) || (lhs == rhs);
  }

  friend bool operator>(const Version& lhs, const Version& rhs) {
    return (rhs < lhs);
  }

  friend bool operator>=(const Version& lhs, const Version& rhs) {
    return (lhs > rhs) || (lhs == rhs);
  }

  Version(int x, int y, int z = 0)
    : ver_major(x), ver_minor(y), ver_patch(z)
  {
  }

  Version()
    : Version(0,0,0)
  {
  }

  Version(unsigned long ver)
    : ver_major((ver >> 24) & 0xFF)
    , ver_minor((ver >> 16) & 0xFF)
    , ver_patch(ver & 0xFFFF)
  {
  }

  std::string str() const {
    std::ostringstream buffer;
    buffer << ver_major << "." << ver_minor << "." << ver_patch;
    return buffer.str();
  }

  long ver_major;
  long ver_minor;
  long ver_patch;
};


/**
 * Designator grammar
 *
 * root ::= <name>
 * root ::= <name> "(" <op> <version> ( "," <op> <version> )* ")"
 * op ::= "<<" | "<=" | "!=" | "==" | ">>" | ">="
 * version ::= <number> "." <number> "." <number>
 */

class Designator {
public:

  Designator(const std::string& str);

  enum Relation {
    LESS_THEN,
    LESS_EQUAL,
    EQUAL,
    NOT_EQUAL,
    GREATER_EQUAL,
    GREATER_THEN
  };

public:
  class Constraint
    : public std::vector< std::pair<Relation, Version> >
  {
    friend std::ostream& operator<<(std::ostream& out,
                                    const Constraint& con)
    {
      static const char *const name[] = {
        "<<", "<=", "==", "!=", ">=", ">>",
      };
      for (auto item: con)
        out << name[item.first] << item.second;
      return out;
    }
  };

  bool version_good(const Version& ver) const;

  std::string plugin;
  Constraint constraint;

private:
  void trace(const std::string& where) const;

  void parse_error(const std::string& prefix) const __attribute__ ((noreturn));
  std::string::value_type peek() const;
  std::string::value_type next();

  Relation parse_relation();
  Version parse_version();
  long parse_number();
  void parse_plugin();
  void parse_root();
  void parse_version_list();
  void skip_space();

  const std::string& input_;
  std::string::const_iterator cur_;

};

#endif /* DESIGNATOR_INCLUDED */
