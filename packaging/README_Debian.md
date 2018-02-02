
Packaging for Debian and Ubuntu
===============================

Each Debian-based Linux distribution has its own packaging folder prefixed
with `deb-`. The code name for each Debian or Ubuntu distribution
is used instead of the version number. At the time of writing (Sep 2015)
the following are supported:

  deb-jessie  Debian 8
  deb-trusty  Ubuntu 14.04
  deb-vivid   Ubuntu 15.04
  deb-wily    Ubuntu 15.10

Combining common and per distribution folders
---------------------------------------------

The folder `deb-common` holds files which are identical for each Debian
or Ubuntu distribution. To get the full directory for packaging both the
common and the per distribution folder has to be copied. For example,
to create the packaging folder for Ubuntu 15.04 (vivid) we do the
following:

  $ cp -a deb-common debian
  $ cp -a deb-vivid/* debian/

The folder `debian` will contain all necessary files to create the package
for Ubuntu 15.04.

Note that the `deb-common` does not contain files such as `changelog`,
`control`, or `rules`. There are also no placeholders to prevent the
`deb-common` to be used and potentially create incorrect packages.

Creating packages
-----------------

The `build_deb.sh` can be used to automate the process creating Debian
packages. It will copy the necessary files, build the packages, and
write a log file which can be used to debug.

Example usage of `bulid_deb.sh`, when inside source of MySQL Router:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make package_source
    $ sh ../packaging/build_deb.sh mysql-router-2.0.2.tar.gz

The log file will be available inside the `DebianBuild` folder:

    DebianBuild/debuild.log

When done, the `build_deb.sh` will show the packages which were build:

    Debian packages:
    mysql-router_2.0.2-1ubuntu14.04_amd64.deb
    mysql-router-dev_2.0.2-1ubuntu14.04_amd64.deb

Installation Layout
-------------------

The following tree shows the installation layout of MySQL Router on
Debian-based 64-bit distributions.

    ├── etc
    │   ├── init.d
    │   │   └── mysqlrouter
    │   └── mysql
    │       └── mysqlrouter.conf
    └── usr
        ├── lib
        │   └── x86_64-linux-gnu
        │       ├── libmysqlharness.so.0
        │       ├── libmysqlrouter.so.0
        │       └── mysqlrouter
        │           ├── metadata_cache.so
        │           ├── keepalive.so
        │           └── routing.so
        ├── bin
        │   └── mysqlrouter
        └── share
            ├── doc
            │   └── mysql-router
            │       ├── changelog.Debian.gz
            │       ├── copyright
            │       ├── sample_mysqlrouter.conf
            │       ├── License.txt.gz
            │       └── README.txt
            └── lintian
                └── overrides
                    └── mysql-router

