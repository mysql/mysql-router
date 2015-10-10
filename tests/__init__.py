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
tests

This Python modules contains various helper functions/classes
for testing using Python.
"""

import argparse
import os
import subprocess
import sys
import unittest

# Ignore file extensions
IGNORE_FILE_EXT = ['.o', '.pyc', '.pyo', '.ini.in', '.cfg.in', '.cfg', '.html', '.css', '.ini', '.gitignore']

# Folders not checked, relative to repository root
IGNORE_FOLDERS = [
    os.path.join('mysql_harness', 'ext'),
    os.path.join('packaging'),
    os.path.join('internal'),
    os.path.join('.git'),
    os.path.join('.idea'),
    os.path.join('build'),
]

# Files not checked, relative to repository root
IGNORE_FILES = [
    os.path.join('License.txt'),
    os.path.join('mysql_harness', 'License.txt'),
    os.path.join('mysql_harness', 'Doxyfile.in'),
]


class ArgHelpFormatter(argparse.HelpFormatter):

    """Help message formatter combining default formatters of argparse"""

    def _fill_text(self, text, width, indent):
        if sys.version_info.major == 2:
            return ''.join([indent +
                           line for line in text.splitlines(True)])
        else:
            return ''.join(indent +
                           line for line in text.splitlines(keepends=True))

    def _get_help_string(self, action):
        help = action.help
        if '%(default)' not in action.help:
            if action.default is not argparse.SUPPRESS:
                defaulting_nargs = [argparse.OPTIONAL, argparse.ZERO_OR_MORE]
                if action.option_strings or action.nargs in defaulting_nargs:
                    help += ' (default: %(default)s)'
        return help


def check_cmake_source_dir(source_dir):
    """Check if the given CMake source directory is usable

    This function checks whether the given CMake source directory
    is valid by checking the presense of certain files.

    Returns the absolute path of source_dir if valid; None if
    not valid.
    """
    path = None
    try:
        path = os.path.abspath(source_dir)
        if not os.path.isdir(path):
            raise ValueError
        elif not os.path.isfile(os.path.join(path, 'License.txt')):
            raise ValueError
    except ValueError:
        return None
    else:
        return path


def get_arguments(description=None, extra_args=[]):
    """Get command line arguments for test scripts

    This function gets command line arguments for a test script.
    It will display the given description in the --help output
    and will use the list of dictionaries for setting up
    extra arguments.

    This function will exit the script with an error when
    there is an issue with an argument.

    Returns the result of ArgumentParser.parse_args().
    """
    parser = argparse.ArgumentParser(
        formatter_class=ArgHelpFormatter,
        description=description)

    # Get some defaults from environment variables
    try:
        cmake_source_dir = os.environ['CMAKE_SOURCE_DIR']
    except KeyError:
        cmake_source_dir = os.getcwd()

    # The generic group: argument for all test scripts
    generic = parser.add_argument_group('generic arguments')
    arg_cmake_source_dir = generic.add_argument(
        '--cmake-source-dir',
        default=cmake_source_dir,
        metavar='PATH',
        help="Full path of the top level CMake source tree",
        dest='cmake_source_dir')

    # Adding extra, test script specific arguments
    if extra_args:
        extra = parser.add_argument_group('specific arguments')

        for extra_arg in extra_args:
            extra.add_argument(**extra_arg)

    args = parser.parse_args()

    try:
        if not check_cmake_source_dir(args.cmake_source_dir):
            raise argparse.ArgumentError(
                arg_cmake_source_dir,
                "Invalid source directory; was '%s'"
                % args.cmake_source_dir)
    except argparse.ArgumentError as exc:
        err = sys.exc_info()[1]
        parser.error(str(err))

    return args


def get_path_root(path):
    """Get the root of given path"""
    if not os.sep in path:
        return path
    else:
        return path[:path.index(os.sep)]


def seek_needle(fp, needle):
    """Find needle in a file

    Iterate over fp and stop when needle is found in the
    line. We leave the file object as is, so it is possible
    to use it further.

    Return the line which contains the line.
    """
    line = None
    for curr_line in fp:
        if needle in curr_line:
            line = curr_line
            break

    return line

def git_tracked(file_path):
    """Check if a file is tracked by Git

    Return True if file is tracked; False other wise
    """
    devnull = open(os.devnull, 'w')

    proc = subprocess.Popen(['git', 'ls-files', '--error-unmatch',
                            file_path], stdout=devnull, stderr=devnull)
    ret = proc.wait()
    return proc.wait() == 0

def is_in_ignored_folder(path):
    for ignored_folder in IGNORE_FOLDERS:
        if path.startswith(ignored_folder):
            return True
    return False
