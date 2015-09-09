# Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.
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

# Originally part of MySQL Server and Adapted for MySQL Router.

# The purpose of this file is to set the default installation layout.
#
# The current choices of installation layout are:
#
#  STANDALONE
#    Build with prefix=/usr/local/mysql, create tarball with install prefix="."
#    and relative links.  Windows zip uses the same tarball layout but without
#    the build prefix.
#
#  RPM, SLES
#    Build as per default RPM layout, with prefix=/usr
#    Note: The layout for ULN RPMs differs, see the "RPM" section.
#
#  DEB
#    Build as per default Debian layout, with prefix=/usr
#    Note: previous layout is now named DEBSRV4
#
#  DEBSRV4
#    Build as per STANDALONE, prefix=/opt/mysql/server-$major.$minor
#
#  SVR4
#    Solaris package layout suitable for pkg* tools, prefix=/opt/mysql/mysql
#
#  FREEBSD, GLIBC, OSX, TARGZ
#    Build with prefix=/usr/local/mysql, create tarball with install prefix="."
#    and relative links.
#
#  WIN
#     Windows zip : same as tarball layout but without the build prefix
#
# To force a directory layout, use -DINSTALL_LAYOUT=<layout>.
#
# The default is STANDALONE.
#
# Note : At present, RPM and SLES layouts are similar. This is also true
#        for layouts like FREEBSD, GLIBC, OSX, TARGZ. However, they provide
#        opportunity to fine-tune deployment for each platform without
#        affecting all other types of deployment.
#
# There is the possibility to further fine-tune installation directories.
# Several variables can be overwritten:
#
# - INSTALL_BINDIR          (directory with client executables and scripts)
# - INSTALL_SBINDIR         (directory with mysqld)
# - INSTALL_SCRIPTDIR       (several scripts, rarely used)
#
# - INSTALL_LIBDIR          (directory with client end embedded libraries)
# - INSTALL_PLUGINDIR       (directory for plugins)
#
# - INSTALL_INCLUDEDIR      (directory for MySQL headers)
#
# - INSTALL_DOCDIR          (documentation)
# - INSTALL_DOCREADMEDIR    (readme and similar)
# - INSTALL_MANDIR          (man pages)
# - INSTALL_INFODIR         (info pages)
#
# - INSTALL_SHAREDIR        (location of aclocal/mysql.m4)
# - INSTALL_MYSQLSHAREDIR   (MySQL character sets and localized error messages)
# - INSTALL_MYSQLTESTDIR    (mysqlrouter-test)
# - INSTALL_SQLBENCHDIR     (sql-bench)
# - INSTALL_SUPPORTFILESDIR (various extra support files)
#
# - INSTALL_MYSQLDATADIR    (data directory)
# - INSTALL_SECURE_FILE_PRIVDIR (--secure-file-priv directory)
#
# When changing this page,  _please_ do not forget to update public Wiki
# http://forge.mysql.com/wiki/CMake#Fine-tuning_installation_paths

IF(NOT INSTALL_LAYOUT)
  SET(DEFAULT_INSTALL_LAYOUT "STANDALONE")
ENDIF()

SET(INSTALL_LAYOUT "${DEFAULT_INSTALL_LAYOUT}"
CACHE STRING "Installation directory layout. Options are: TARGZ (as in tar.gz installer), WIN (as in zip installer), STANDALONE, RPM, DEB, DEBSRV4, SVR4, FREEBSD, GLIBC, OSX, SLES")

IF(UNIX)
  IF(INSTALL_LAYOUT MATCHES "RPM" OR
     INSTALL_LAYOUT MATCHES "SLES" OR
     INSTALL_LAYOUT MATCHES "DEB")
    SET(default_prefix "/usr")
  ELSEIF(INSTALL_LAYOUT MATCHES "DEBSVR4")
    SET(default_prefix "/opt/mysql/mysqlrouter-${PROJECT_VERSION}")
    # This is required to avoid "cpack -GDEB" default of prefix=/usr
    SET(CPACK_SET_DESTDIR ON)
  ELSEIF(INSTALL_LAYOUT MATCHES "SVR4")
    SET(default_prefix "/opt/mysql/mysqlrouter")
  ELSE()
    SET(default_prefix "/usr/local/mysqlrouter")
  ENDIF()
  IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    SET(CMAKE_INSTALL_PREFIX ${default_prefix}
      CACHE PATH "install prefix" FORCE)
  ENDIF()
  SET(VALID_INSTALL_LAYOUTS "RPM" "DEB" "DEBSVR4" "SVR4" "FREEBSD" "GLIBC" "OSX" "TARGZ" "SLES" "STANDALONE")
  LIST(FIND VALID_INSTALL_LAYOUTS "${INSTALL_LAYOUT}" ind)
  IF(ind EQUAL -1)
    MESSAGE(FATAL_ERROR "Invalid INSTALL_LAYOUT parameter:${INSTALL_LAYOUT}."
    " Choose between ${VALID_INSTALL_LAYOUTS}" )
  ENDIF()

  SET(SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/etc/mysql"
    CACHE PATH "config directory (for mysqlrouter.ini)")
  MARK_AS_ADVANCED(SYSCONFDIR)
ENDIF()

IF(WIN32)
  SET(VALID_INSTALL_LAYOUTS "TARGZ" "STANDALONE" "WIN")
  LIST(FIND VALID_INSTALL_LAYOUTS "${INSTALL_LAYOUT}" ind)
  IF(ind EQUAL -1)
    MESSAGE(FATAL_ERROR "Invalid INSTALL_LAYOUT parameter:${INSTALL_LAYOUT}."
    " Choose between ${VALID_INSTALL_LAYOUTS}" )
  ENDIF()
ENDIF()

#
# DEFAULT_SECURE_FILE_PRIV_DIR
#
IF(INSTALL_LAYOUT MATCHES "STANDALONE" OR
   INSTALL_LAYOUT MATCHES "WIN")
  SET(secure_file_priv_path "")
ELSEIF(INSTALL_LAYOUT MATCHES "RPM" OR
       INSTALL_LAYOUT MATCHES "SLES" OR
       INSTALL_LAYOUT MATCHES "SVR4" OR
       INSTALL_LAYOUT MATCHES "DEB" OR
       INSTALL_LAYOUT MATCHES "DEBSVR4")
  SET(secure_file_priv_path "/var/lib/mysqlrouter-files")
ELSE()
  SET(secure_file_priv_path "${default_prefix}/mysqlrouter-files")
ENDIF()

#
# STANDALONE layout
#
SET(INSTALL_BINDIR_STANDALONE           "bin")
SET(INSTALL_SBINDIR_STANDALONE          "bin")
SET(INSTALL_SCRIPTDIR_STANDALONE        "scripts")
#
SET(INSTALL_LIBDIR_STANDALONE           "lib")
SET(INSTALL_PLUGINDIR_STANDALONE        "lib/mysqlrouter")
#
SET(INSTALL_INCLUDEDIR_STANDALONE       "include")
#
SET(INSTALL_DOCDIR_STANDALONE           "docs")
SET(INSTALL_DOCREADMEDIR_STANDALONE     ".")
SET(INSTALL_MANDIR_STANDALONE           "man")
SET(INSTALL_INFODIR_STANDALONE          "docs")
#
SET(INSTALL_SHAREDIR_STANDALONE         "share")
SET(INSTALL_MYSQLSHAREDIR_STANDALONE    "share")
SET(INSTALL_MYSQLTESTDIR_STANDALONE     "mysqlrouter-test")
SET(INSTALL_SQLBENCHDIR_STANDALONE      ".")
SET(INSTALL_SUPPORTFILESDIR_STANDALONE  "support-files")
#
SET(INSTALL_MYSQLDATADIR_STANDALONE     "data")
SET(INSTALL_PLUGINTESTDIR_STANDALONE    ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_STANDALONE ${secure_file_priv_path})

#
# WIN layout
#
SET(INSTALL_BINDIR_WIN           "bin")
SET(INSTALL_SBINDIR_WIN          "bin")
SET(INSTALL_SCRIPTDIR_WIN        "scripts")
#
SET(INSTALL_LIBDIR_WIN           "lib")
SET(INSTALL_PLUGINDIR_WIN        "lib/mysqlrouter")
#
SET(INSTALL_INCLUDEDIR_WIN       "include")
#
SET(INSTALL_DOCDIR_WIN           "docs")
SET(INSTALL_DOCREADMEDIR_WIN     ".")
SET(INSTALL_MANDIR_WIN           "man")
SET(INSTALL_INFODIR_WIN          "docs")
#
SET(INSTALL_SHAREDIR_WIN         "share")
SET(INSTALL_MYSQLSHAREDIR_WIN    "share")
SET(INSTALL_MYSQLTESTDIR_WIN     "mysqlrouter-test")
SET(INSTALL_SQLBENCHDIR_WIN      ".")
SET(INSTALL_SUPPORTFILESDIR_WIN  "support-files")
#
SET(INSTALL_MYSQLDATADIR_WIN     "data")
SET(INSTALL_PLUGINTESTDIR_WIN    ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_WIN ${secure_file_priv_path})

#
# FREEBSD layout
#
SET(INSTALL_BINDIR_FREEBSD           "bin")
SET(INSTALL_SBINDIR_FREEBSD          "bin")
SET(INSTALL_SCRIPTDIR_FREEBSD        "scripts")
#
SET(INSTALL_LIBDIR_FREEBSD           "lib")
SET(INSTALL_PLUGINDIR_FREEBSD        "lib/mysqlrouter")
#
SET(INSTALL_INCLUDEDIR_FREEBSD       "include")
#
SET(INSTALL_DOCDIR_FREEBSD           "docs")
SET(INSTALL_DOCREADMEDIR_FREEBSD     ".")
SET(INSTALL_MANDIR_FREEBSD           "man")
SET(INSTALL_INFODIR_FREEBSD          "docs")
#
SET(INSTALL_SHAREDIR_FREEBSD         "share")
SET(INSTALL_MYSQLSHAREDIR_FREEBSD    "share")
SET(INSTALL_MYSQLTESTDIR_FREEBSD     "mysqlrouter-test")
SET(INSTALL_SQLBENCHDIR_FREEBSD      ".")
SET(INSTALL_SUPPORTFILESDIR_FREEBSD  "support-files")
#
SET(INSTALL_MYSQLDATADIR_FREEBSD     "data")
SET(INSTALL_PLUGINTESTDIR_FREEBSD    ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_FREEBSD ${secure_file_priv_path})

#
# GLIBC layout
#
SET(INSTALL_BINDIR_GLIBC           "bin")
SET(INSTALL_SBINDIR_GLIBC          "bin")
SET(INSTALL_SCRIPTDIR_GLIBC        "scripts")
#
SET(INSTALL_LIBDIR_GLIBC           "lib")
SET(INSTALL_PLUGINDIR_GLIBC        "lib/mysqlrouter")
#
SET(INSTALL_INCLUDEDIR_GLIBC       "include")
#
SET(INSTALL_DOCDIR_GLIBC           "docs")
SET(INSTALL_DOCREADMEDIR_GLIBC     ".")
SET(INSTALL_MANDIR_GLIBC           "man")
SET(INSTALL_INFODIR_GLIBC          "docs")
#
SET(INSTALL_SHAREDIR_GLIBC         "share")
SET(INSTALL_MYSQLSHAREDIR_GLIBC    "share")
SET(INSTALL_MYSQLTESTDIR_GLIBC     "mysqlrouter-test")
SET(INSTALL_SQLBENCHDIR_GLIBC      ".")
SET(INSTALL_SUPPORTFILESDIR_GLIBC  "support-files")
#
SET(INSTALL_MYSQLDATADIR_GLIBC     "data")
SET(INSTALL_PLUGINTESTDIR_GLIBC    ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_GLIBC ${secure_file_priv_path})

#
# OSX layout
#
SET(INSTALL_BINDIR_OSX           "bin")
SET(INSTALL_SBINDIR_OSX          "bin")
SET(INSTALL_SCRIPTDIR_OSX        "scripts")
#
SET(INSTALL_LIBDIR_OSX           "lib")
SET(INSTALL_PLUGINDIR_OSX        "lib/mysqlrouter")
#
SET(INSTALL_INCLUDEDIR_OSX       "include")
#
SET(INSTALL_DOCDIR_OSX           "docs")
SET(INSTALL_DOCREADMEDIR_OSX     ".")
SET(INSTALL_MANDIR_OSX           "man")
SET(INSTALL_INFODIR_OSX          "docs")
#
SET(INSTALL_SHAREDIR_OSX         "share")
SET(INSTALL_MYSQLSHAREDIR_OSX    "share")
SET(INSTALL_MYSQLTESTDIR_OSX     "mysqlrouter-test")
SET(INSTALL_SQLBENCHDIR_OSX      ".")
SET(INSTALL_SUPPORTFILESDIR_OSX  "support-files")
#
SET(INSTALL_MYSQLDATADIR_OSX     "data")
SET(INSTALL_PLUGINTESTDIR_OSX    ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_OSX ${secure_file_priv_path})

#
# TARGZ layout
#
SET(INSTALL_BINDIR_TARGZ           "bin")
SET(INSTALL_SBINDIR_TARGZ          "bin")
SET(INSTALL_SCRIPTDIR_TARGZ        "scripts")
#
SET(INSTALL_LIBDIR_TARGZ           "lib")
SET(INSTALL_PLUGINDIR_TARGZ        "lib/mysqlrouter")
#
SET(INSTALL_INCLUDEDIR_TARGZ       "include")
#
SET(INSTALL_DOCDIR_TARGZ           "docs")
SET(INSTALL_DOCREADMEDIR_TARGZ     ".")
SET(INSTALL_MANDIR_TARGZ           "man")
SET(INSTALL_INFODIR_TARGZ          "docs")
#
SET(INSTALL_SHAREDIR_TARGZ         "share")
SET(INSTALL_MYSQLSHAREDIR_TARGZ    "share")
SET(INSTALL_MYSQLTESTDIR_TARGZ     "mysqlrouter-test")
SET(INSTALL_SQLBENCHDIR_TARGZ      ".")
SET(INSTALL_SUPPORTFILESDIR_TARGZ  "support-files")
#
SET(INSTALL_MYSQLDATADIR_TARGZ     "data")
SET(INSTALL_PLUGINTESTDIR_TARGZ    ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_TARGZ ${secure_file_priv_path})

#
# RPM layout
#
# See "packaging/rpm-uln/mysql-5.5-libdir.patch" for the differences
# which apply to RPMs in ULN (Oracle Linux), that patch file will
# be applied at build time via "rpmbuild".
#
SET(INSTALL_BINDIR_RPM                  "bin")
SET(INSTALL_SBINDIR_RPM                 "sbin")
SET(INSTALL_SCRIPTDIR_RPM               "bin")
#
IF(ARCH_64BIT)
  SET(INSTALL_LIBDIR_RPM                "lib64")
  SET(INSTALL_PLUGINDIR_RPM             "lib64/mysqlrouter/plugin")
ELSE()
  SET(INSTALL_LIBDIR_RPM                "lib")
  SET(INSTALL_PLUGINDIR_RPM             "lib/mysqlrouter/plugin")
ENDIF()
#
SET(INSTALL_INCLUDEDIR_RPM              "include/mysql")
#
#SET(INSTALL_DOCDIR_RPM                 unset - installed directly by RPM)
#SET(INSTALL_DOCREADMEDIR_RPM           unset - installed directly by RPM)
SET(INSTALL_INFODIR_RPM                 "share/info")
SET(INSTALL_MANDIR_RPM                  "share/man")
#
SET(INSTALL_SHAREDIR_RPM                "share")
SET(INSTALL_MYSQLSHAREDIR_RPM           "share/mysql-router")
SET(INSTALL_MYSQLTESTDIR_RPM            "share/mysql-router-test")
SET(INSTALL_SQLBENCHDIR_RPM             "")
SET(INSTALL_SUPPORTFILESDIR_RPM         "share/mysql-router")
#
SET(INSTALL_MYSQLDATADIR_RPM            "/var/lib/mysqlrouter")
SET(INSTALL_PLUGINTESTDIR_RPM           ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_RPM     ${secure_file_priv_path})

#
# SLES layout
#
SET(INSTALL_BINDIR_SLES                  "bin")
SET(INSTALL_SBINDIR_SLES                 "sbin")
SET(INSTALL_SCRIPTDIR_SLES               "bin")
#
IF(ARCH_64BIT)
  SET(INSTALL_LIBDIR_SLES                "lib64")
  SET(INSTALL_PLUGINDIR_SLES             "lib64/mysqlrouter/plugin")
ELSE()
  SET(INSTALL_LIBDIR_SLES                "lib")
  SET(INSTALL_PLUGINDIR_SLES             "lib/mysqlrouter/plugin")
ENDIF()
#
SET(INSTALL_INCLUDEDIR_SLES              "include/mysqlrouter")
#
#SET(INSTALL_DOCDIR_SLES                 unset - installed directly by SLES)
#SET(INSTALL_DOCREADMEDIR_SLES           unset - installed directly by SLES)
SET(INSTALL_INFODIR_SLES                 "share/info")
SET(INSTALL_MANDIR_SLES                  "share/man")
#
SET(INSTALL_SHAREDIR_SLES                "share")
SET(INSTALL_MYSQLSHAREDIR_SLES           "share/mysql-router")
SET(INSTALL_MYSQLTESTDIR_SLES            "share/mysqlrouter-test")
SET(INSTALL_SQLBENCHDIR_SLES             "")
SET(INSTALL_SUPPORTFILESDIR_SLES         "share/mysql")
#
SET(INSTALL_MYSQLDATADIR_SLES            "/var/lib/mysqlrouter")
SET(INSTALL_PLUGINTESTDIR_SLES           ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_SLES     ${secure_file_priv_path})

#
# DEB layout
#
SET(INSTALL_BINDIR_DEB                  "bin")
SET(INSTALL_SBINDIR_DEB                 "sbin")
SET(INSTALL_SCRIPTDIR_DEB               "bin")
#
IF(ARCH_64BIT)
  SET(INSTALL_LIBDIR_DEB                "lib/x86_64-linux-gnu")
  SET(INSTALL_PLUGINDIR_DEB             "lib/x86_64-linux-gnu/mysqlrouter")
ELSE()
  SET(INSTALL_LIBDIR_DEB                "lib")
  SET(INSTALL_PLUGINDIR_DEB             "lib/mysqlrouter")
ENDIF()
#
SET(INSTALL_INCLUDEDIR_DEB              "include/mysql/router")
#
SET(INSTALL_DOCDIR_DEB                  "share/mysql-router/docs")
SET(INSTALL_DOCREADMEDIR_DEB            "share/mysql-router/docs")
SET(INSTALL_MANDIR_DEB                  "share/man")
SET(INSTALL_INFODIR_DEB                 "share/mysql-router/docs")
#
SET(INSTALL_SHAREDIR_DEB                "share")
SET(INSTALL_MYSQLSHAREDIR_DEB           "share/mysql-router")
SET(INSTALL_MYSQLTESTDIR_DEB            "lib/mysql-router-test")
SET(INSTALL_SQLBENCHDIR_DEB             "lib/mysql-router")
SET(INSTALL_SUPPORTFILESDIR_DEB         "share/mysql-router")
#
SET(INSTALL_MYSQLDATADIR_DEB            "/var/lib/mysqlrouter")
SET(INSTALL_PLUGINTESTDIR_DEB           ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_DEB     ${secure_file_priv_path})

#
# DEBSVR4 layout
#
SET(INSTALL_BINDIR_DEBSVR4              "bin")
SET(INSTALL_SBINDIR_DEBSVR4             "bin")
SET(INSTALL_SCRIPTDIR_DEBSVR4           "scripts")
#
SET(INSTALL_LIBDIR_DEBSVR4              "lib")
SET(INSTALL_PLUGINDIR_DEBSVR4           "lib/mysqlrouter")
#
SET(INSTALL_INCLUDEDIR_DEBSVR4          "include")
#
SET(INSTALL_DOCDIR_DEBSVR4              "docs")
SET(INSTALL_DOCREADMEDIR_DEBSVR4        ".")
SET(INSTALL_MANDIR_DEBSVR4              "man")
SET(INSTALL_INFODIR_DEBSVR4             "docs")
#
SET(INSTALL_SHAREDIR_DEBSVR4            "share")
SET(INSTALL_MYSQLSHAREDIR_DEBSVR4       "share")
SET(INSTALL_MYSQLTESTDIR_DEBSVR4        "mysqlrouter-test")
SET(INSTALL_SQLBENCHDIR_DEBSVR4         ".")
SET(INSTALL_SUPPORTFILESDIR_DEBSVR4     "support-files")
#
SET(INSTALL_MYSQLDATADIR_DEBSVR4        "/var/lib/mysqlrouter")
SET(INSTALL_PLUGINTESTDIR_DEBSVR4       ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_DEBSVR4 ${secure_file_priv_path})

#
# SVR4 layout
#
SET(INSTALL_BINDIR_SVR4                 "bin")
SET(INSTALL_SBINDIR_SVR4                "bin")
SET(INSTALL_SCRIPTDIR_SVR4              "scripts")
#
SET(INSTALL_LIBDIR_SVR4                 "lib")
SET(INSTALL_PLUGINDIR_SVR4              "lib/plugin")
#
SET(INSTALL_INCLUDEDIR_SVR4             "include")
#
SET(INSTALL_DOCDIR_SVR4                 "docs")
SET(INSTALL_DOCREADMEDIR_SVR4           ".")
SET(INSTALL_MANDIR_SVR4                 "man")
SET(INSTALL_INFODIR_SVR4                "docs")
#
SET(INSTALL_SHAREDIR_SVR4               "share")
SET(INSTALL_MYSQLSHAREDIR_SVR4          "share")
SET(INSTALL_MYSQLTESTDIR_SVR4           "mysqlrouter-test")
SET(INSTALL_SQLBENCHDIR_SVR4            ".")
SET(INSTALL_SUPPORTFILESDIR_SVR4        "support-files")
#
SET(INSTALL_MYSQLDATADIR_SVR4           "/var/lib/mysqlrouter")
SET(INSTALL_PLUGINTESTDIR_SVR4          ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_SVR4    ${secure_file_priv_path})


# Clear cached variables if install layout was changed
IF(OLD_INSTALL_LAYOUT)
  IF(NOT OLD_INSTALL_LAYOUT STREQUAL INSTALL_LAYOUT)
    SET(FORCE FORCE)
  ENDIF()
ENDIF()
SET(OLD_INSTALL_LAYOUT ${INSTALL_LAYOUT} CACHE INTERNAL "")

# Set INSTALL_FOODIR variables for chosen layout (for example, INSTALL_BINDIR
# will be defined  as ${INSTALL_BINDIR_STANDALONE} by default if STANDALONE
# layout is chosen)
FOREACH(var BIN SBIN LIB MYSQLSHARE SHARE PLUGIN INCLUDE SCRIPT DOC MAN
  INFO MYSQLTEST SQLBENCH DOCREADME SUPPORTFILES MYSQLDATA PLUGINTEST
  SECURE_FILE_PRIV)
  SET(INSTALL_${var}DIR  ${INSTALL_${var}DIR_${INSTALL_LAYOUT}}
  CACHE STRING "${var} installation directory" ${FORCE})
  MARK_AS_ADVANCED(INSTALL_${var}DIR)
ENDFOREACH()

#
# Set DEFAULT_SECURE_FILE_PRIV_DIR
# This is used as default value for --secure-file-priv
#
IF(INSTALL_SECURE_FILE_PRIVDIR)
  SET(DEFAULT_SECURE_FILE_PRIV_DIR "\"${INSTALL_SECURE_FILE_PRIVDIR}\""
      CACHE INTERNAL "default --secure-file-priv directory" FORCE)
ELSE()
  SET(DEFAULT_SECURE_FILE_PRIV_DIR \"\"
      CACHE INTERNAL "default --secure-file-priv directory" FORCE)
ENDIF()
