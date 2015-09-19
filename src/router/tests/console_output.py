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


from datetime import date
import os
import unittest
from subprocess import check_output
import sys

from tests import get_arguments

APP_NAME = 'mysqlrouter'
APP_BIN = os.path.join(os.environ['STAGE_DIR'], 'bin', APP_NAME)


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

    def test_help_copyright(self):
        """WL8480: --help contains valid copyright notice
        """
        if date.today().year == 2015:
            years = "2015,"
        else:
            years = "2015, %d," % date.today().year

        output = check_output([self.app_bin, '--help']).split('\n')
        self.assertTrue(output[0].startswith("Copyright "))
        self.assertTrue(years in output[0])

    def test_help_trademark(self):
        """WL8480: --help contains trademark notice
        """
        needle = "Oracle is a registered trademark "
        output = check_output([self.app_bin, '--help']).split('\n')
        self.assertTrue(output[2].startswith(needle))

    def test_help_config_files_list(self):
        """WL8480: --help lists default configuration files
        """
        output = check_output([self.app_bin, '--help']).split('\n')

        found = False
        files = []
        for line in output:
            if line.startswith("Configuration read"):
                found = True
                continue
            if found:
                if not line.strip():
                    break
                elif line.startswith('  '):
                    files.append(line.strip())

        self.assertTrue(found, "List of configuration files missing")
        self.assertTrue(len(files) > 1,
                        "Failed reading list of configuration files")

    def test_help_usage(self):
        """WL8480: --help basic usage of mysqlrouter command"""

        output = check_output([self.app_bin, '--help']).split("\n")

        options = (
            "[-v|--version]",
            "[-h|--help]",
            "[-c|--config=<path>]",
            "[-a|--extra-config=<path>]"
        )

        found = False
        usage_lines = []
        for line in output:
            if line.startswith("Options: "):
              break
            elif line.startswith("Usage: ") or found:
                usage_lines.append(line)
                found = True
        usage_lines = '\n'.join(usage_lines)

        self.assertTrue(found, "Line with usage not found")

        self.assertTrue(all([needle in usage_lines for needle in usage_lines]), "Basic command options wrong")

    def test_help_option_description(self):
        """WL8480: --help listing all options and description
        """
        output = check_output([self.app_bin, '--help']).split('\n')

        found = False
        options = []
        desc = []
        names_line = None
        for line in output:
            if not found and line.startswith("Options:"):
                found = True
                continue
            if found and not line.strip():
                if names_line and desc:
                    options.append((names_line, desc))
            elif line.startswith('  -'):
                if names_line and desc:
                    options.append((names_line, desc))
                names_line = line
                desc = []
            elif names_line and line.startswith('    '):
                desc.append(line)

        self.assertTrue(found, "List options not available")

        self.assertTrue(options >= 3,
                        "Failed reading list options with descriptions")

        for names, desc in options:
            self.assertTrue(desc,
                        "Option '%s' did not have description" % names)

if __name__ == '__main__':
    args = get_arguments(description=__doc__)

    unittest.main(argv=[sys.argv[0]], verbosity=3)
