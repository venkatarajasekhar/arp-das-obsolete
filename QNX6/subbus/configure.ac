#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([subbus], 4.0, allen@huarp.harvard.edu)
AM_INIT_AUTOMAKE(subbus, 4.0)
AC_CONFIG_SRCDIR([src/subbus.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.
AC_CHECK_LIB([nort], [nl_error])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_CHECK_FUNCS([strerror])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
