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

from tests import (
    get_arguments, get_path_root, git_tracked,
    IGNORE_FILE_EXT, IGNORE_FILES, is_in_ignored_folder)


class TestCopyright(unittest.TestCase):

    root_path = ''

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
                if not str(year) in years:
                    return path + " (year %s missing, file changed!)" % str(year)
                return None                

        return path + " (copyright notice not present)"

    def test_copyright_notice(self):
        """WL8400: Check copyright and years in all relevant files"""

        failures = []

        for base, dirs, files in os.walk(self.root_path):
            if not base == self.root_path:
                relative_base = base.replace(self.root_path + os.sep, '')
            else:
                relative_base = ''

            if is_in_ignored_folder(relative_base):
                continue

            for filename in files:
                fullpath = os.path.join(base, filename)
                if is_in_ignored_folder(relative_base):
                    continue
                if not git_tracked(fullpath):
                    continue

                relative = os.path.join(relative_base, filename)
                if relative in IGNORE_FILES:
                    continue

                if not any([filename.endswith(ext) for ext in IGNORE_FILE_EXT]):
                    if os.path.getsize(fullpath):
                        result = self._check_copyright_presence(fullpath)
                        if result:
                            failures.append(result)

        if failures:
            self.fail("Check copyright in following files: \n" + '\n'.join(failures) + "\n")

if __name__ == '__main__':
    args = get_arguments(description=__doc__)

    TestCopyright.root_path = args.cmake_source_dir
    unittest.main(argv=[sys.argv[0]], verbosity=3)
