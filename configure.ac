dnl Process this file with autoconf to produce a configure script.

AC_INIT([gnome-mastermind], [0.4.0])
AC_PREREQ([2.5])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([subdir-objects])

AM_CONFIG_HEADER([config.h])

dnl Check for C compiler
AC_LANG([C])
AC_PROG_CC
AM_PROG_CC_STDC
AC_ISC_POSIX

dnl Checks for various programs
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

dnl Next four lines is a hack to prevent libtool checking for C++/F77
m4_undefine([AC_PROG_CXX])
m4_defun([AC_PROG_CXX],[])
m4_undefine([AC_PROG_F77])
m4_defun([AC_PROG_F77],[])

dnl Libtool
AC_DISABLE_STATIC
AC_ENABLE_SHARED
AC_PROG_LIBTOOL

AM_MAINTAINER_MODE
GNOME_MAINTAINER_MODE_DEFINES
GNOME_COMPILE_WARNINGS

dnl pkg-config
PKG_PROG_PKG_CONFIG

dnl GTK
PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= 3.21.4])

AM_PATH_GLIB_2_0
GLIB_GSETTINGS

GM_DEFINES='-DDATA_DIR="\"$(datadir)\"" -DPKGDATA_DIR="\"$(pkgdatadir)\""'
AC_SUBST([GM_DEFINES])

dnl CFLAGS
CFLAGS="$CFLAGS -Wall -pedantic -std=c99 -fno-strict-aliasing -fmessage-length=0 -D_FORTIFY_SOURCE=2"

AM_PROG_CC_C_O

#AM_GNU_GETTEXT
#AM_GNU_GETTEXT_VERSION([0.16.1])
AC_PROG_INTLTOOL([0.50.1])

AC_DEFINE([GETTEXT_PACKAGE], [], [gnome-mastermind])
GETTEXT_PACKAGE=gnome-mastermind
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE")

AM_GLIB_GNU_GETTEXT

gmlocaledir='${prefix}/${DATADIRNAME}/locale'
AC_SUBST(gmlocaledir)

#ALL_LINGUAS="it pl fr sv de en_GB eu id ar pt_BR es"
#AC_SUBST(ALL_LINGUAS)

YELP_HELP_INIT

AC_CONFIG_FILES([  po/Makefile.in
    Makefile
    src/Makefile
    themes/Makefile
    ui/Makefile
    desktop/Makefile
    icons/Makefile
    help/Makefile
])

AC_OUTPUT

