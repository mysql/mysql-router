# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

configure_file(mysql-router.spec.in ${CMAKE_BINARY_DIR}/mysql-router.spec @ONLY)

if(ARCH_64BIT)
  set(RPM_LIBDIR "/usr/lib64")
else()
  set(RPM_LIBDIR "/usr/lib")
endif()

configure_file(mysqlrouter.conf.in ${CMAKE_BINARY_DIR}/packaging/rpm-oel/mysqlrouter.conf @ONLY)
configure_file(mysqlrouter.init ${CMAKE_BINARY_DIR}/packaging/rpm-oel/mysqlrouter.init @ONLY)
configure_file(mysqlrouter.service ${CMAKE_BINARY_DIR}/packaging/rpm-oel/mysqlrouter.service @ONLY)
configure_file(mysqlrouter.tmpfiles.d ${CMAKE_BINARY_DIR}/packaging/rpm-oel/mysqlrouter.tmpfiles.d @ONLY)
