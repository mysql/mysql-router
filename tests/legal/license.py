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
This script runs Python unit tests checking whether the license
is mentioned in source and other files. This does not check the
copyright (see copyright.py).

Files and folders can be ignored as well as file extension. See
the class TestLicense members _ingore_files, _ignore_folders
and _ignore_file_ext.

The location of MySQL Router source can be provided using the command
line argument --cmake-source-dir. By default, the current working
directory is used. If the environment variable CMAKE_SOURCE_DIR is
found, it will be use as default.
"""

from datetime import date
from hashlib import sha1
import os
import re
from subprocess import check_output
import sys
from time import strptime
import unittest

from tests import (
    get_arguments, get_path_root, seek_needle, git_tracked,
    IGNORE_FILE_EXT, IGNORE_FILES, is_in_ignored_folder)

EXP_SHORT_LICENSE = """
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
"""

EXP_SHORT_LICENSE_LINES = EXP_SHORT_LICENSE.split("\n")

class TestLicense(unittest.TestCase):

    root_path = ''

    extra_ignored_files = [
        'README.md',
        'README.txt',
        'License.txt',
        os.path.join('src', 'router', 'include', 'README.txt'),
        os.path.join('mysql_harness', 'README.txt'),
        os.path.join('mysql_harness', 'License.txt'),
        os.path.join('mysql_harness', 'Doxyfile.in'),
        ]

    def setUp(self):
        self.root_path = os.path.abspath(self.root_path)

    def _check_license_presence(self, path):
        """Check if short license text is present

        Returns True if all is OK; False otherwise.
        """
        ext = os.path.splitext(path)[1]
        errmsg = "License problem in %s (license line: %%d)" % path

        with open(path, 'rb') as fp:
            # Go to the line containing the copyright statement
            line = seek_needle(fp, 'Copyright (c)')
            if not line:
                return path + " (Copyright notice not found)"

            # Always blank line after copyright
            try:
                line = next(fp)
            except StopIteration:
                return path + " (no blank line after copyright)"
            if line[0] == '#':
                line = line[1:]
            if line.strip():
                return path + " (no blank line after copyright)"

            # Now check license part
            curr_line = 1
            for line in fp:
                # Remove hash sign if present
                if line[0] == '#':
                    line = line[1:]
                if curr_line == len(EXP_SHORT_LICENSE_LINES) - 1:
                    # We are at the end; skip blank
                    break
                if not EXP_SHORT_LICENSE_LINES[curr_line].strip() == line.strip():
                    return path + " (error line %d in short license)" % curr_line
                curr_line += 1
            if not 13 == curr_line:
                return path + " (short license not 13 lines)"

        return None

    def test_short_license_notice(self):
        """WL8400: Check short license notice"""

        ignored_files = IGNORE_FILES + self.extra_ignored_files

        failures = []

        for base, dirs, files in os.walk(self.root_path):
            if base != self.root_path:
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
                if relative in ignored_files:
                    continue

                if not any([filename.endswith(ext) for ext in IGNORE_FILE_EXT]):
                    if os.path.getsize(fullpath):
                        result = self._check_license_presence(fullpath)
                        if result:
                            failures.append(result)

        if failures:
            self.fail("Check license in following files: \n" + '\n'.join(failures) + "\n")

    def test_license_txt(self):
        """WL8400: Check content of License.txt"""
        with open(os.path.join(self.root_path, 'License.txt')) as fp:
            hash = sha1(fp.read()).hexdigest()

        self.assertEqual('06877624ea5c77efe3b7e39b0f909eda6e25a4ec',
                         hash)

    def test_readme_license_foss(self):
        """WL8400: Check FOSS exception in README.txt"""
        with open(os.path.join(self.root_path, 'README.txt')) as fp:
            line = seek_needle(fp, 'MySQL FOSS License Exception')
            self.assertTrue(
                line is not None,
                "Could not find start of FOSS exception")

            nr_lines = 16
            lines = []
            while nr_lines > 0:
                lines.append(next(fp))
                nr_lines -= 1
            hash = sha1(''.join(lines)).hexdigest()

            self.assertEqual(
                'd319794f726e1d8dae88227114e30761bc98b11f',
                hash,
                "FOSS exception in README.txt changed?")

    def test_readme_gpl_disclamer(self):
        """WL8400: Check GPL Disclaimer in README.txt"""
        with open(os.path.join(self.root_path, 'README.txt')) as fp:
            line = seek_needle(fp, 'GPLv2 Disclaimer')
            self.assertTrue(
                line is not None,
                "Could not find start of GPL Disclaimer exception")

            nr_lines = 7
            lines = []
            while nr_lines > 0:
                lines.append(next(fp))
                nr_lines -= 1
            hash = sha1(''.join(lines)).hexdigest()

            self.assertEqual(
                '7ea8fbbe1fcdf8965a3ee310f14e6eb7cb1543d0',
                hash,
                "GPL Disclaimer in README.txt changed?")

if __name__ == '__main__':
    args = get_arguments(description=__doc__)

    TestLicense.root_path = args.cmake_source_dir
    unittest.main(argv=[sys.argv[0]], verbosity=3)
