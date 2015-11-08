m4_define([libpgdb2_major_version], [0])
m4_define([libpgdb2_minor_version], [0])
m4_define([libpgdb2_micro_version], [1])
m4_define([libpgdb2_interface_age], [1])
# If you need a modifier for the version number. 
# Normally empty, but can be used to make "fixup" releases.
m4_define([libpgdb2_extraversion], [])

dnl libtool versioning from libpgdb2
m4_define([libpgdb2_current], [m4_eval(100 * libpgdb2_minor_version + libpgdb2_micro_version - libpgdb2_interface_age)])
m4_define([libpgdb2_binary_age], [m4_eval(100 * libpgdb2_minor_version + libpgdb2_micro_version)])
m4_define([libpgdb2_revision], [libpgdb2_interface_age])
m4_define([libpgdb2_age], [m4_eval(libpgdb2_binary_age - libpgdb2_interface_age)])
m4_define([libpgdb2_version], [libpgdb2_major_version().libpgdb2_minor_version().libpgdb2_micro_version()libpgdb2_extraversion()])


AC_INIT([pgdb2], [0.0.1],
        [http://github.com/jgarzik/pgdb2/])

AC_PREREQ(2.60)
AC_CONFIG_SRCDIR([lib/db.cc])
AM_INIT_AUTOMAKE([gnu])
AC_CONFIG_HEADERS([pgdb2-config.h])

LIBPGDB2_MAJOR_VERSION=libpgdb2_major_version
LIBPGDB2_MINOR_VERSION=libpgdb2_minor_version
LIBPGDB2_MICRO_VERSION=libpgdb2_micro_version
LIBPGDB2_INTERFACE_AGE=libpgdb2_interface_age

# ABI version
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
LIBPGDB2_CURRENT=libpgdb2_current
LIBPGDB2_REVISION=libpgdb2_revision
LIBPGDB2_AGE=libpgdb2_age

AC_SUBST(LIBPGDB2_CURRENT)
AC_SUBST(LIBPGDB2_REVISION)
AC_SUBST(LIBPGDB2_AGE)

LT_INIT

AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXXCPP
AC_PROG_INSTALL
AC_PROG_OBJC
AC_PROG_LN_S
AC_PROG_MKDIR_P
AC_PROG_SED
AC_PATH_TOOL(AR, ar)
AC_PATH_TOOL(RANLIB, ranlib)
AC_PATH_TOOL(STRIP, strip)
PKG_PROG_PKG_CONFIG

AC_CHECK_FUNCS(posix_fadvise)

AC_LANG_PUSH([C++])

case $host in
  *mingw*)
     LDFLAGS+=" -static-libgcc -static-libstdc++"
     ;;
esac

AC_CONFIG_FILES([
    lib/Makefile
    include/Makefile
    test/Makefile
    Makefile])
AC_OUTPUT

