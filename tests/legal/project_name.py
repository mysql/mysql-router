# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

"""
This script runs Python unit tests checking the name of the
project in various files.
"""

from datetime import date
import os
import re
from subprocess import check_output
import sys
from time import strptime
import unittest

from tests import get_arguments

PROJECT_NAME = "MySQL Router"


class TestProjectName(unittest.TestCase):

    root_path = ''

    # Relative to source directory
    _files_with_project_name = [
        'README.txt',
    ]

    def setUp(self):
        self.root_path = os.path.abspath(self.root_path)

    def test_readme_txt(self):
        """WL8400: Check project name in README.txt"""

        with open(os.path.join(self.root_path, 'README.txt')) as fp:
            found = False
            first_line = fp.readline()
            self.assertTrue(first_line.startswith(PROJECT_NAME),
                "README.txt first does not start with '%s'" %
                PROJECT_NAME)

            for line in fp:
                if line.startswith('This is a release of'):
                    self.assertTrue(PROJECT_NAME in line)
                    found = True
                    break
            self.assertTrue(
                found,
                "Project name '%s' not found in README.txt" % PROJECT_NAME)


    def test_part_of(self):
        """WL8400: Check if the 'part of'-clause is present"""

        # File to check with needle for getting the line
        files_to_check = [
            (os.path.join(self.root_path, 'README.txt'),
             'This is a release of'),
        ]

        cmake_file = os.path.join(self.root_path, 'cmake', 'version.cmake')
        part_of_text = ""
        with open(os.path.join(self.root_path,  cmake_file)) as fp:
            for line in fp.readlines():
                if 'set(PROJECT_PARTOF_TEXT ' in line:
                    part_of_text = re.findall('"([^"]*)"', line)[0]
                    self.assertTrue(part_of_text.startswith('MySQL '))
                    break

        self.assertTrue(part_of_text != "")

        i = 0
        for file_to_check, needle in files_to_check:
            with open(file_to_check) as fp:
                for line in fp:
                    if needle in line:
                        break
                self.assertTrue(
                    "part of " + part_of_text in line,
                    "part of-clause wrong in README.txt (index=%d)" % i)
            i += 1


    def test_settings_cmake(self):
        """WL8400: Check if project name in settings.cmake

        Note that this is not full checking settings.cmake.
        """

        cmake_file = os.path.join(self.root_path, 'cmake', 'settings.cmake')
        with open(os.path.join(self.root_path,  cmake_file)) as fp:
            found = 0
            exp_found = 2
            for line in fp.readlines():
                if 'set(MYSQL_PROJECT_NAME ' in line:
                    self.assertTrue('"MySQLRouter"' in line,
                                    "MYSQL_PROJECT_NAME is incorrect")
                    found += 1
                elif 'set(MYSQL_ROUTER_TARGET ' in line:
                    self.assertTrue('"mysqlrouter"' in line,
                                    "MYSQL_ROUTER_TARGET is incorrect")
                    found += 1

            self.assertEqual(exp_found, found,
                             "Failed checking settings.cmake")


if __name__ == '__main__':
    args = get_arguments(description=__doc__)

    TestProjectName.root_path = args.cmake_source_dir
    unittest.main(argv=[sys.argv[0]], verbosity=3)
