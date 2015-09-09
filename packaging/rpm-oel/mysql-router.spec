# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
#
# MySQL MySQL Router is licensed under the terms of the GPLv2
# <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>, like most
# MySQL Connectors. There are special exceptions to the terms and
# conditions of the GPLv2 as it is applied to this software, see the
# FOSS License Exception
# <http://www.mysql.com/about/legal/licensing/foss-exception.html>.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

%{!?src_base:                 %global src_base mysql-router}

%global src_dir               %{src_base}-%{version}

%if 0%{?suse_version} == 1315
%global dist            .sles12
%endif

%if 0%{?suse_version} == 1110
%global dist            .sles11
%endif

%if 0%{?commercial}
%global license_type    Commercial
%global product_suffix  -commercial
%else
%global license_type    GPLv2
%endif

%{!?with_systemd:       %global systemd 1}
%{?el6:                 %global systemd 0}

Summary:       MySQL Router
Name:          mysql-router%{?product_suffix}
Version:       2.0.0
Release:       1%{?dist}
License:       Copyright (c) 2015 Oracle and/or its affiliates. All rights reserved. Under %{?license_type} license as shown in the Description field.
Group:         Development/Libraries
URL:           https://dev.mysql.com/downloads/router/
Source0:       https://cdn.mysql.com/Downloads/router/mysql-router-%{?commercial:commercial-}%{version}.tar.gz
Source1:       mysqlrouter.service
Source2:       mysqlrouter.tmpfiles.d
Source3:       mysqlrouter.ini
BuildRequires: python-devel
%if 0%{?commercial}
Provides:      mysql-router = %{version}-%{release}
Obsoletes:     mysql-router < %{version}-%{release}
%endif
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
%if 0%{?systemd}
Requires(post):   systemd
Requires(preun):  systemd
Requires(postun): systemd
%else
Requires(post):   /sbin/chkconfig
Requires(preun):  /sbin/chkconfig
Requires(preun):  /sbin/service
%endif

%description
The MySQL(TM) Router software delivers a fast, multi-threaded way of
routing connections from MySQL Clients to MySQL Servers. MySQL is a
trademark of Oracle.

The MySQL software has Dual Licensing, which means you can use the
MySQL software free of charge under the GNU General Public License
(http://www.gnu.org/licenses/). You can also purchase commercial MySQL
licenses from Oracle and/or its affiliates if you do not wish to be
bound by the terms of the GPL. See the chapter "Licensing and Support"
in the manual for further info.

The MySQL web site (http://www.mysql.com/) provides the latest news
and information about the MySQL software. Also please see the
documentation and the manual for more information.

%package     -n mysql-router%{?product_suffix}-devel
Summary:        Development header files and libraries for MySQL Router
Group:          Applications/Databases
%if 0%{?commercial}
Provides:       mysql-router-devel = %{version}-%{release}
Obsoletes:      mysql-router-devel < %{version}-%{release}
%endif
%description -n mysql-router%{?product_suffix}-devel
This package contains the development header files and libraries
necessary to develop MySQL Router applications.

%prep
%setup -q
sed -i -e 's/^install/#install/' src/router/CMakeLists.txt

%build
mkdir release && pushd release
cmake .. -DINSTALL_LAYOUT=RPM -DINSTALL_LIBDIR="%{_lib}"
make %{?_smp_mflags} VERBOSE=1
popd

%install
rm -rf %{buildroot}
pushd release
make DESTDIR=%{buildroot} install

install -d -m 0755 %{buildroot}/%{_localstatedir}/log/mysql

install -D -p -m 0644 %{SOURCE1} %{buildroot}%{_unitdir}/mysqlrouter.service
install -D -p -m 0644 %{SOURCE2} %{buildroot}%{_tmpfilesdir}/mysqlrouter.conf
install -D -p -m 0644 %{SOURCE3} %{buildroot}%{_sysconfdir}/mysql/mysqlrouter.ini

# remove some unwanted files
rm -rf %{buildroot}%{_includedir}
rm -rf %{buildroot}/usr/lib/libmysqlharness.{a,so}

%clean
rm -rf %{buildroot}

%pre
/usr/sbin/groupadd -g 27 -o -r mysql >/dev/null 2>&1 || :
/usr/sbin/useradd -M -N -g mysql -o -r -d /var/lib/mysql -s /bin/false \
    -c "MySQL Server" -u 27 mysql >/dev/null 2>&1 || :

%post
/sbin/ldconfig
%if 0%{?systemd}
%systemd_post mysqlrouter.service
%else
/sbin/chkconfig --add mysqlrouter
%endif

%preun
%if 0%{?systemd}
%systemd_preun mysqlrouter.service
%else
if [ "$1" = 0 ]; then
    /sbin/service mysqlrouter stop >/dev/null 2>&1 || :
    /sbin/chkconfig --del mysqlrouter
fi
%endif

%postun
%if 0%{?systemd}
%systemd_postun_with_restart mysqlrouter.service
%else
if [ $1 -ge 1 ]; then
    /sbin/service mysqlrouter condrestart >/dev/null 2>&1 || :
fi
%endif
/sbin/ldconfig

%files
%defattr(-, root, root, -)
%doc License.txt README.txt
%dir %{_sysconfdir}/mysql
%config(noreplace) %{_sysconfdir}/mysql/mysqlrouter.ini
%{_sbindir}/mysqlrouter
%if 0%{?systemd}
%{_unitdir}/mysqlrouter.service
%{_tmpfilesdir}/mysqlrouter.conf
%else
%{_sysconfdir}/init.d/mysqlrouter
%endif
# todo: need support for libdir = /usr/lib64, not just /usr/lib
/usr/lib/libmysqlharness.so.0
%dir /usr/lib/mysqlrouter
/usr/lib/mysqlrouter/logger.so
/usr/lib/mysqlrouter/keepalive.so
%dir %attr(755, mysql, mysql) %{_localstatedir}/log/mysql

%changelog
* Wed Aug 19 2015 Balasubramanian Kandasamy <balasubramanian.kandasamy@oracle.com> - 2.0.0-1
- Initial version

