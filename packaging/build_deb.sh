#!/bin/sh

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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

# When no arguments are given, show some help
if [ $# -eq 0 ]; then
  echo "Usage: `basename $0` /path/to/mysql-router-2.0.0.tar.gz <debuild arguments>"
  exit 0
fi

DEFAULT_DEBUILD_ARGS="-us -uc"  # no signing

ME=$0
SOURCETAR=$1
shift
DEBUILDARGS=${@:-$DEFAULT_DEBUILD_ARGS}
WORKDIR=`pwd`/DebianBuild
LOG=$WORKDIR/debuild.log

if [ "$SOURCETAR" = "" ] || [ ! -f $SOURCETAR ]; then
  echo "Source TAR archive not available"
  exit 1
fi
SOURCETAR=`realpath $SOURCETAR`
REPODIR=`dirname $ME`/..
REPODIR=`realpath $REPODIR`

# Check if we are where we are supposed to be
if [ ! -f $REPODIR/src/router/src/router_app.cc ]; then
  echo "This script is supposed to run within the MySQL Router source repository."
  exit 1
fi

# Get and check support for Debian/Ubuntu release support
DEBCODE=`lsb_release -c -s`
if [ $? -ne 0 ]; then
  echo "Failed getting relesae information. Make sure lsb_release is avialable."
  exit 1
fi
DEBCOMMONDIR=$REPODIR/packaging/deb-common
DEBINFODIR=$REPODIR/packaging/deb-$DEBCODE
if [ ! -d $DEBINFODIR ]; then
  echo "Debian/Ubuntu with code name $DEBINFODIR is not supported."
  exit 1
fi

echo "Using source TAR: $SOURCETAR"

ORIGDIR=`pwd`
rm -Rf $WORKDIR
mkdir $WORKDIR
cd $WORKDIR

# Rename source TAR to comply to Debian standards
cp $SOURCETAR .
TARBASE=`basename $SOURCETAR .tar.gz`
VERSION=${TARBASE##*-}
PKGNAME=${TARBASE%%-$VERSION*}

DEBSRCTAR=${PKGNAME}_$VERSION.orig.tar.gz
mv $TARBASE.tar.gz $DEBSRCTAR
echo "Renamed TAR to $DEBSRCTAR"

# Unpack Source and copy the appropriated Debian package files as well as common files
tar xzf $DEBSRCTAR
cp -a $DEBCOMMONDIR $TARBASE/debian
cp -a $DEBINFODIR/* $TARBASE/debian/
echo "Copied `basename $DEBINFODIR`"

# Build Debian package
cd $TARBASE
if [ ! -f debian/rules ]; then
  echo "Wrong location: can not start debuild (in `pwd`)"
  exit 1
fi
echo "Process logged to $LOG"
echo "Building package using 'debuild $DEBUILDARGS'.."
debuild $DEBUILDARGS 2>$LOG 1>&2
if [ $? -ne 0 ]; then
  tail -n30 $LOG
else
  lintian=`sed -ne '1,/^Now running lintian...\$/d; /Finished running lintian.\$/,\$ d; p' $LOG`

  if [ -n "$lintian" ]; then
    echo
    echo "Lintian errors and warnings"
    echo "---------------------------"
    echo "$lintian"
    echo
  fi

  echo "Debian packages:"
  for debfile in `ls $WORKDIR/*.deb`; do
    cp -a $debfile $ORIGDIR
    echo "`basename $debfile`"
  done
fi
