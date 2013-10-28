AC_PREREQ([2.59])
AC_INIT([observer], [1.0.0])
AC_CONFIG_AUX_DIR([aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([observer], [1.0.0])

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_LANG_C
AC_PROG_CC
AC_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_GCC_TRADITIONAL
ACX_PTHREAD

dnl define pthread cflag
AC_SUBST(PTHREAD_CFLAGS, [-pthread])

AC_ARG_ENABLE([debug],
    [  --enable-debug                   Turn on debugging],
    [case "${enableval}" in
      yes) debug=true; CFLAGS="-O0 -g3 -ggdb";;
      no)  debug=false; CFLAGS="-DNDEBUG";;
      *)   AC_MSG_ERROR([bad value ${enableval} for --enable-debug]);;
      esac],
    [debug=false; CFLAGS="-DNDEBUG"])
AM_CONDITIONAL([NDEBUG], [test "x$debug" = xfalse])

AC_C_INLINE
AC_C_CONST

LT_INIT

dnl check size of types
AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([size_t])

dnl check headers
AC_HEADER_STDC
AC_CHECK_HEADERS([limits.h stdio.h stdlib.h string.h unistd.h pthread.h])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
])

AC_OUTPUT
