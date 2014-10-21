#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([MIO],
        [0.1],
        [ban@herbesfolles.org],
        [libmio])
AC_CONFIG_SRCDIR([mio/mio.c])
AC_CONFIG_MACRO_DIR([build/m4])
AC_CONFIG_AUX_DIR([build/aux])
AM_INIT_AUTOMAKE([1.11.1 -Wall foreign])
AC_CONFIG_HEADERS([config.h])

AM_SILENT_RULES([yes])

# 
# CURRENT:REVISION:AGE
# 
# Remember to bump library version before releases; here the rules:
# If you have:
#   * not changed the interface (bug fixes):          CURRENT:REV+1:AGE
#   * augmented the interface (new things):           CURRENT+1:0:AGE+1
#   * broken the interface (removed/changed things):  CURRENT+1:0:0
MIO_LTVERSION="0:0:0"
AC_SUBST([MIO_LTVERSION])

# Checks for programs.
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_PREREQ([2.2.0])
LT_INIT
AC_PROG_CC

# check for gtk-doc
GTK_DOC_CHECK([1.9])

# Checks for libraries.
PKG_CHECK_MODULES([GLIB], [glib-2.0], [have_glib=yes], [have_glib=no])

# Checks for header files.
AC_CHECK_HEADERS([string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CHECK_FUNC([vsnprintf], [have_vsnprintf=yes], [have_vsnprintf=no])
AC_CHECK_DECL([va_copy],
              [have_va_copy=yes],
              [AC_CHECK_DECL([__va_copy],
                             [AC_DEFINE([va_copy], [__va_copy], [va_copy wrapper])
                              have_va_copy=yes],
                             [have_va_copy=no],
                             [#include <stdarg.h>])],
              [#include <stdarg.h>])

# Conditional configuration
AC_ARG_ENABLE([glib],
              [AS_HELP_STRING([--enable-glib],
                              [Whether to enable GLib support @<:@default=auto@:>@])],
              [enable_glib=$enableval],
              [enable_glib=auto])
dnl determine the real value of enable_glib
AS_IF([test "x$enable_glib" = xauto],
      [enable_glib=$have_glib])

dnl check configuration consistency and abort on error
AS_IF([test "x$enable_glib" = xyes],
      [AS_IF([test "x$have_glib" != xyes],
             [AC_MSG_ERROR([GLib support enabled but GLib was not found])])
       AC_SUBST([GLIB_PKG], [glib-2.0])
       AC_DEFINE([HAVE_GLIB], [1], [Whether to enable GLib support])],
      [AS_IF([test "x$have_vsnprintf" != xyes -o "x$have_va_copy" != xyes],
             [AC_MSG_ERROR([vsnprintf() or va_copy() cannot be found])])
       AC_DEFINE([HAVE_VSNPRINTF], [1], [Whether vsnprintf() is available])
       AC_DEFINE([HAVE_VA_COPY], [1], [Whether va_copy() is available])])
dnl Make conditionals
AM_CONDITIONAL([HAVE_GLIB],   [test "x$have_glib" = xyes])
AM_CONDITIONAL([ENABLE_GLIB], [test "x$enable_glib" = xyes])

# Output.
AC_CONFIG_FILES([Makefile
                 mio/Makefile
                 tests/Makefile
                 docs/Makefile
                 docs/reference/Makefile
                 docs/reference/mio/Makefile
                 docs/reference/mio/version.xml
                 mio.pc])
AC_OUTPUT
