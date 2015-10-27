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

# Show some help
if [ "$1" == "--help" ]; then
  echo "Usage: `basename $0` [--commercial] <rpmbuild arguments>"
  exit 0
fi

DEFAULT_RPMBUILD_ARGS="-ba"

ME=$0
COM=0
GPL=1
if [ "$1" == "--commercial" ]; then
  COM=1
  GPL=0
fi
shift
RPMBUILDARGS=${@:-$DEFAULT_RPMBUILD_ARGS}

cmake .. -DGPL=${GPL}
make package_source
SPEC="mysql-router.spec"

if [ ! -f ./$SPEC ]; then
  echo "Spec file not available."
  exit 1
fi

WORKDIR=`pwd`/RPMBuild
LOG=${WORKDIR}/rpmbuild.log

if [ $COM -eq 1 ]; then
  SOURCETAR=`ls mysql-router-commercial*.tar.gz`
else
  SOURCETAR=`ls mysql-router-*.tar.gz`
fi

if [ "${SOURCETAR}" = "" ] || [ ! -f ${SOURCETAR} ]; then
  echo "Source TAR archive not available"
  exit 1
fi

if [ ! -d ../mysql-server ]; then
  echo "MySQL Server not available in ../mysql-server"
  exit 1
fi
MYSQL_SERVER=`realpath ../mysql-server`
echo "Using MySQL libraries from ${MYSQL_SERVER}"

SOURCETAR=`realpath ${SOURCETAR}`
REPODIR=`dirname ${ME}`/..
REPODIR=`realpath ${REPODIR}`

set -e

echo "Preparing workdir"
rm -Rf ${WORKDIR}
mkdir -p ${WORKDIR}/{SOURCES,BUILD,SPECS,RPMS,SRPMS}

echo "Copying spec file(s)"
cp -a ${SPEC} ${WORKDIR}/SPECS

echo "Copying source files $SOURCETAR"
cp -a $SOURCETAR ${WORKDIR}/SOURCES
cp -a packaging/rpm-oel/mysqlrouter.* ${WORKDIR}/SOURCES

rpmbuild -v --define="_topdir ${WORKDIR}" --define="with_mysql ${MYSQL_SERVER}" \
  --define="_tmppath ${WORKDIR}" --define="commercial $COM" ${RPMBUILDARGS} ${SPEC}

mv ${WORKDIR}/RPMS/*.rpm .
mv ${WORKDIR}/SRPMS/*.rpm .

set +e


