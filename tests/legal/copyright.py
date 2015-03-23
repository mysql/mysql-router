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
This script runs Python unit tests checking the copyright notice in
all files in the MySQL Router source repository.

Files and folders can be ignored as well as file extension. See
the class TestCopyright members _ingore_files, _ignore_folders
and _ignore_file_ext.

The location of MySQL Router source can be provided using the command
line argument --cmake-source-dir. By default, the current working
directory is used. If the environment variable CMAKE_SOURCE_DIR is
found, it will be use as default.
"""

from datetime import date
import os
import re
from subprocess import check_output
import sys
from time import strptime
import unittest

from tests import get_arguments, get_path_root, git_tracked


class TestCopyright(unittest.TestCase):

    root_path = ''
    _ignore_file_ext = ['.o', '.pyc', '.pyo']

    # Folders not checked, relative to root_path
    _ignore_folders = [
        os.path.join('.git'),
        os.path.join('.idea'),
        os.path.join('build'),
        os.path.join('gtest'),
        os.path.join('boost'),
    ]

    # Files not checked, relative to root_path
    _ignore_files = [
        os.path.join('.gitignore'),
        os.path.join('License.txt'),
    ]

    def setUp(self):
        self.root_path = os.path.abspath(self.root_path)

    def _check_copyright_presence(self, path, year=None):
        """Check if copyright is presence and has correct year

        Returns True if all is OK; False otherwise.
        """
        p = re.compile(r'(\d{4}),')
        copyright_notice = re.compile(
            r'.*Copyright \(c\) (?:(\d{4}), )(?:(\d{4}), ){0,1}'
            r'Oracle and/or its affiliates. All rights reserved.$')

        if not year:
            git_output = check_output(
                ['git', 'log', '--format=%ci', path])
            if git_output:
                year = strptime(
                    ' '.join(git_output.split(' ')[0:2]),
                    "%Y-%m-%d %H:%M:%S").tm_year

            # We seem to have an uncommitted file
            if not year:
                year = date.today().year

        for line in open(path, 'rb'):
            match = copyright_notice.match(line)
            if match:
                years = match.groups()
                self.assertTrue(
                    str(year) in years,
                    "Check year(s) in '{file}' ({year})".format(
                        file=path, year=year))
                return

        self.fail("No copyright notice found in "
                  "file '{file}'".format(file=path))

    def test_copyright_notice(self):
        """WL8400: Check copyright and years in all relevant files"""

        for base, dirs, files in os.walk(self.root_path):
            if base != self.root_path:
                relative_base = base.replace(self.root_path + os.sep, '')
            else:
                relative_base = ''
            if get_path_root(relative_base) in self._ignore_folders:
                continue

            for filename in files:
                fullpath = os.path.join(base, filename)
                if not git_tracked(fullpath):
                    continue

                relative = os.path.join(relative_base, filename)
                if relative in self._ignore_files:
                    continue

                _, ext = os.path.splitext(filename)
                if ext not in self._ignore_file_ext:
                    if os.path.getsize(fullpath):
                        self._check_copyright_presence(fullpath)


if __name__ == '__main__':
    args = get_arguments(description=__doc__)

    TestCopyright.root_path = args.cmake_source_dir
    unittest.main(argv=[sys.argv[0]], verbosity=3)
