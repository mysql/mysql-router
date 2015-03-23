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


import os
import unittest
from subprocess import check_output
import sys

APP_NAME = 'mysqlrouter'
SRC_PATH = os.path.join('src', 'router')
APP_BIN = os.path.join(SRC_PATH, 'src', APP_NAME)


class TestConsoleOutput(unittest.TestCase):

    def setUp(self):
        try:
            self.bin_dir = os.environ['CMAKE_BINARY_DIR']
            print(self.bin_dir)
        except KeyError:
            self.bin_dir = os.getcwd()

        # Check executable
        self.app_bin = os.path.join(self.bin_dir, APP_BIN)
        if not (os.path.isfile(self.app_bin) and
                os.access(self.app_bin, os.X_OK)):
            self.fail("Binary %s not found" % self.app_bin)

    def test_start_first_line(self):
        """WL8400: First line output start with 'MySQL Router'
        """
        output = check_output([self.app_bin])
        self.assertTrue(output.startswith("MySQL Router"))


if __name__ == '__main__':
    unittest.main(verbosity=3)
