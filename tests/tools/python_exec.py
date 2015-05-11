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

# Test if unit testing works for Python

import unittest
import sys


class TestPythonInterpreter(unittest.TestCase):

    def test_version(self):
        """WL8400: Require Python >=2.7 or or >=3.3
        """
        if sys.version_info.major == 2:
            self.assertTrue(sys.version_info.minor >= 7)
        if sys.version_info.major == 3:
            self.assertTrue(sys.version_info.minor >= 3)


if __name__ == '__main__':
    unittest.main()
