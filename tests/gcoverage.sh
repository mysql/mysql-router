#!/bin/sh

# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#
# Shell script gathering coverage information and generating HTML report
#
# Depends on lcov.pl and genhtml which should be best installed through the
# Linux distribution's packaging system.
#

LCOV=`which lcov`
GENHTML=`which genhtml`
GCOVINFO="mysqlrouter_coverage.info"
FORCE=0
HTMLOUTDIR=  # no default

errecho() {
  >&2 echo "$@";
}

# Check dependencies
if [ -z $LCOV ]; then
  errecho "Please make sure 'lcov' is installed and available."
  exit 1
elif [ -z $GENHTML ]; then
  errecho "Please make sure 'genhtml' is installed and available."
  exit 1
fi

while getopts "ho:f" option; do
  case "$option" in
    h)
      echo "Usage: $0 [-o /path/to/html/output/] [-f]"
      echo "  -f     Force using output folder by deleting its content"
      echo "  -o     Output folder for genhtml"
      exit 0
      ;;
    o)
      HTMLOUTDIR=$OPTARG
      ;;
    f)
      FORCE=1
      ;;
    *)
      echo "Unknown option $option"
      exit 2
      ;;
  esac
done

gcno_cnt=`find . -name *.gcno 2>/dev/null | grep -c gcno`
if [ $gcno_cnt -eq 0 ]; then
    errecho "No gcno files found. CMake option -DENABLE_GCOV=yes should be used; tests and/or binary should be executed."
    exit 1
fi

# Create the HTML Output folder
if [ -z $HTMLOUTDIR ]; then
  errecho "Use -o option to specify where the HTML report should be stored."
  exit 1
fi
mkdir $HTMLOUTDIR 2>/dev/null
if [ $? -ne 0 ]; then
  # First whether the output folder is empty, if it exists
  if [ -d $HTMLOUTDIR ] && [ "`ls -A $HTMLOUTDIR`" != "" ]; then
    if [ $FORCE -eq 1 ]; then
      rm -Rf $HTMLOUTDIR/*
    else
      errecho "Make sure HTML output folder is empty or does not exists. Use -f to force emptying folder."
      exit 1
    fi
  fi
  if [ ! -d $HTMLOUTDIR ]; then
    errecho "Failed creating HTML ouptut folder. Please check $HTMLOUTDIR."
    exit 1
  fi
fi

# Gather coverage information
rm $GCOVINFO 2>/dev/null
$LCOV -q -b ./src -d ./src -c -o $GCOVINFO
if [ $? -ne 0 ]; then
  errecho "Failed executing $LCOV while gathering coverage information"
  exit 1
fi
if [ ! -s $GCOVINFO ]; then
  errecho "Coverage information file not available."
  errecho "See possible warnings/errors in above messages."
  exit 1
fi

# Remove folders for which we do not want to generate coverage statistics
$LCOV -q --remove $GCOVINFO "/usr*" "*mysql_harness*" "*tests/helpers*" "ext/*" "generated/protobuf/*"  -o $GCOVINFO
if [ $? -ne 0 ]; then
  errecho "Failed executing $LCOV while removing folders from coverage information"
  exit 1
fi

# Generate HTML report
$GENHTML -q -o $HTMLOUTDIR -t "MySQL Route Code Coverage" $GCOVINFO
if [ $? -ne 0 ]; then
  errecho "Failed executing $GENHTML while generating HTML report"
  exit 1
fi

echo "Done. HTML report in folder: $HTMLOUTDIR"
