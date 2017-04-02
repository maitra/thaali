AC_INIT(thaali, 0.4.0)
AC_CONFIG_SRCDIR(src/thaali.c)
AM_INIT_AUTOMAKE

AM_CONFIG_HEADER(config.h)



AC_PROG_CC
AC_PATH_XTRA

dnl Are we going to enable the debugging output ??
AC_ARG_ENABLE(debug,
AC_HELP_STRING([--enable-debug], [Enable runtime debugging (default: no)]),
        debug=$enableval, debug=no)

if test x$no_x = xyes ; then
	AC_MSG_ERROR([X development libraries not found])
fi

if test "$debug" = yes; then
        AC_DEFINE(DEBUG,,[Enable verbose debugging output])
fi

CFLAGS="$CFLAGS $X_CFLAGS"
#x_includes="-I$x_includes"
LDFLAGS="$LDFLAGS $X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"
#x_libraries="$X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"

AC_SUBST(x_includes)
AC_SUBST(x_libraries)

AC_OUTPUT([
Makefile
src/Makefile
])
