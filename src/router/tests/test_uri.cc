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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <exception>

#define private public
#include "mysqlrouter/uri.h"

using std::get;
using ::testing::StrEq;
using ::testing::ContainerEq;
using ::testing::IsEmpty;
using mysqlrouter::URI;
using mysqlrouter::URIError;
using mysqlrouter::URIQuery;
using mysqlrouter::URIAuthority;
using mysqlrouter::URIPath;
using mysqlrouter::URIQuery;

using mysqlrouter::URI;
using mysqlrouter::URIError;
using mysqlrouter::URIQuery;
using mysqlrouter::URIAuthority;
using mysqlrouter::URIPath;
using mysqlrouter::t_parse_scheme;
using mysqlrouter::t_parse_authority;
using mysqlrouter::t_parse_path;
using mysqlrouter::t_parse_query;
using mysqlrouter::t_parse_fragment;

class URITests: public ::testing::Test {
protected:
  virtual void SetUp() {
  }
};

TEST_F(URITests, Constructor)
{
  URI u;
  ASSERT_TRUE(u.scheme.empty());
  ASSERT_TRUE(u.host.empty());
  ASSERT_EQ(u.port, 0);
  ASSERT_TRUE(u.username.empty());
  ASSERT_TRUE(u.password.empty());
  ASSERT_TRUE(u.path.empty());
  ASSERT_TRUE(u.query.empty());
  ASSERT_TRUE(u.fragment.empty());
}

TEST_F(URITests, ParseScheme)
{
  URI u;
  ASSERT_THAT(mysqlrouter::t_parse_scheme("ham:"), StrEq("ham"));
  ASSERT_THAT(mysqlrouter::t_parse_scheme("HAM:"), StrEq("ham"));
  ASSERT_THAT(mysqlrouter::t_parse_scheme("MySQL+Fabric:"), StrEq("mysql+fabric"));
  ASSERT_THAT(mysqlrouter::t_parse_scheme("MySQL.Fabric:"), StrEq("mysql.fabric"));
  ASSERT_THAT(mysqlrouter::t_parse_scheme("MySQL-Fabric:"), StrEq("mysql-fabric"));
}

TEST_F(URITests, ParseSchemeFail)
{
  ASSERT_THROW(mysqlrouter::t_parse_scheme("ham"), URIError);
  ASSERT_THROW(mysqlrouter::t_parse_scheme("ham$$:"), URIError);
}

TEST_F(URITests, ParseAuthority)
{
  URIAuthority auth;

  auth = mysqlrouter::t_parse_authority("ham://spam.example.com");
  ASSERT_THAT(get<0>(auth), StrEq("spam.example.com"));
  ASSERT_EQ(get<1>(auth), 0);
  ASSERT_TRUE(get<2>(auth).empty());
  ASSERT_TRUE(get<3>(auth).empty());

  auth = mysqlrouter::t_parse_authority("ham://spam.example.com");
  ASSERT_THAT(get<0>(auth), StrEq("spam.example.com"));

  auth = mysqlrouter::t_parse_authority("ham://scott@spam.example.com/");
  ASSERT_THAT(get<0>(auth), StrEq("spam.example.com"));
  ASSERT_THAT(get<2>(auth), StrEq("scott"));
  ASSERT_TRUE(get<3>(auth).empty());

  auth = mysqlrouter::t_parse_authority("ham://scott:@spam.example.com/");
  ASSERT_THAT(get<0>(auth), StrEq("spam.example.com"));
  ASSERT_THAT(get<2>(auth), StrEq("scott"));
  ASSERT_TRUE(get<3>(auth).empty());

  auth = mysqlrouter::t_parse_authority("ham://:@spam.example.com");
  ASSERT_THAT(get<0>(auth), StrEq("spam.example.com"));
  ASSERT_TRUE(get<2>(auth).empty());
  ASSERT_TRUE(get<3>(auth).empty());

  auth = mysqlrouter::t_parse_authority("ham://scott:tiger@spam.example.com:3306/");
  ASSERT_THAT(get<0>(auth), StrEq("spam.example.com"));
  ASSERT_EQ(get<1>(auth), 3306);
  ASSERT_THAT(get<2>(auth), StrEq("scott"));
  ASSERT_THAT(get<3>(auth), StrEq("tiger"));

  auth = mysqlrouter::t_parse_authority("ham://spam.example.com:/");
  ASSERT_EQ(get<1>(auth), 0);
  auth = mysqlrouter::t_parse_authority("ham://spam.example.com:3306/");
  ASSERT_EQ(get<1>(auth), 3306);
}

TEST_F(URITests, ParseAuthorityFail)
{
  ASSERT_THROW(mysqlrouter::t_parse_authority("ham"), URIError);
  ASSERT_THROW(mysqlrouter::t_parse_authority("ham://spam.example.com:999999/"), URIError);
  ASSERT_THROW(mysqlrouter::t_parse_authority("ham://:3306/"), URIError);
}

TEST_F(URITests, ParseAuthorityEmpty)
{
  URIAuthority a = mysqlrouter::t_parse_authority("ham://");
  ASSERT_THAT(get<0>(a), StrEq(""));
  a = mysqlrouter::t_parse_authority("ham:///");
  ASSERT_THAT(get<0>(a), StrEq(""));
}

TEST_F(URITests, ParsePath)
{
  URIPath p;
  p = mysqlrouter::t_parse_path("ham://scott:tiger@spam.example.com:3306/the/way/to/go");
  ASSERT_THAT(p.at(0), StrEq("the"));
  ASSERT_THAT(p.at(1), StrEq("way"));
  ASSERT_THAT(p.at(2), StrEq("to"));
  ASSERT_THAT(p.at(3), StrEq("go"));
  ASSERT_THROW(p.at(4), std::out_of_range);
  p.clear();
  
  p = mysqlrouter::t_parse_path("ham://scott:tiger@spam.example.com:3306/withslashatend/");
  ASSERT_THAT(p.at(0), StrEq("withslashatend"));
  ASSERT_THROW(p.at(1), std::out_of_range);
  p.clear();
  
  p = mysqlrouter::t_parse_path("ham://scott:tiger@spam.example.com:3306/double//slash/");
  ASSERT_THAT(p.at(0), StrEq("double"));
  ASSERT_THAT(p.at(1), StrEq("slash"));
  ASSERT_THROW(p.at(2), std::out_of_range);
  p.clear();
  
  p = mysqlrouter::t_parse_path("file:///path/to/file");
  ASSERT_THAT(p.at(2), StrEq("file"));
  p.clear();
  p = mysqlrouter::t_parse_path("ham://example.com");
  ASSERT_THROW(p.at(0), std::out_of_range);
  p.clear();
  
  p = mysqlrouter::t_parse_path("ham://example.com/path/to/?key1=val2");
  ASSERT_THAT(p.at(0), StrEq("path"));
  ASSERT_THAT(p.at(1), StrEq("to"));
  ASSERT_THROW(p.at(2), std::out_of_range);
  p.clear();
}

TEST_F(URITests, ParsePathFail)
{
  ASSERT_THROW(mysqlrouter::t_parse_path("ham"), URIError);
}

TEST_F(URITests, ParseQuery)
{
  URIQuery q;

  q = mysqlrouter::t_parse_query("ham://example.com?key1=val1&key2=val2", URI::query_delimiter);
  ASSERT_THAT(q["key1"], StrEq("val1"));
  ASSERT_THAT(q["key2"], StrEq("val2"));
  
  q = mysqlrouter::t_parse_query("ham://example.com/path/to/?key1=val1&key2=", '&');
  ASSERT_THAT(q["key1"], StrEq("val1"));
  ASSERT_THAT(q["key2"], StrEq(""));
  
  q = mysqlrouter::t_parse_query("ham://example.com?key1=val1#foo");
  ASSERT_THAT(q["key1"], StrEq("val1"));
}

TEST_F(URITests, ParseQueryFail)
{
  std::string f;
  
  f = mysqlrouter::t_parse_fragment("ham://example.com?key1=val1#foo");
  ASSERT_THAT(f, StrEq("foo"));
  
  f = mysqlrouter::t_parse_fragment("ham://example.com#foo");
  ASSERT_THAT(f, StrEq("foo"));
  
  f = mysqlrouter::t_parse_fragment("ham://example.com#");
  ASSERT_TRUE(f.empty());
  
  f = mysqlrouter::t_parse_fragment("ham://example.com");
  ASSERT_TRUE(f.empty());
}

TEST_F(URITests, ParseFragmentFail)
{
  ASSERT_THROW(mysqlrouter::t_parse_fragment("ham"), URIError);
}

TEST_F(URITests, ConstructorWithURI)
{
  URI u("ham://scott:tiger@host.example.com:3306/path/to/sys?key1=val1");
  ASSERT_THAT(u.scheme, StrEq("ham"));
  ASSERT_THAT(u.username, StrEq("scott"));
  ASSERT_THAT(u.password, StrEq("tiger"));
  ASSERT_THAT(u.host, StrEq("host.example.com"));
  ASSERT_EQ(u.port, 3306);
  ASSERT_THAT(u.path.at(0), StrEq("path"));
  ASSERT_THAT(u.path.at(1), StrEq("to"));
  ASSERT_THAT(u.path.at(2), StrEq("sys"));
  ASSERT_THAT(u.query["key1"], StrEq("val1"));
}

TEST_F(URITests, ConstructorWithURIFail)
{
  ASSERT_THROW(new URI("ham$$://scott:tiger@host.example.com:3306/path/to/sys?key1=val1"), URIError);
}

TEST_F(URITests, SetURI)
{
  URI u("ham://scott:tiger@host.example.com:3306/path/to/sys?key1=val1");
  u.set_uri("spam://spamhost.example.com");
  ASSERT_EQ(u.scheme, string("spam"));
  ASSERT_EQ(u.host, string("spamhost.example.com"));
  ASSERT_EQ(u.port, 0);
  ASSERT_EQ(u.username, string());
  ASSERT_EQ(u.password, string());
  ASSERT_THAT(u.path, IsEmpty());
  ASSERT_THAT(u.query, IsEmpty());
  ASSERT_EQ(u.fragment, string());
}
