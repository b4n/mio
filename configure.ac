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
AM_INIT_AUTOMAKE([1.11.1 -Wall -Werror foreign])
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
LT_PREREQ([2.2.0])
LT_INIT
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([GLIB], [glib-2.0])

# Checks for header files.
AC_CHECK_HEADERS([string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 mio/Makefile
                 tests/Makefile])
AC_OUTPUT
