AC_PREREQ([2.67])
AC_INIT([pidgin-im-pusher], [0.07], [http://code.google.com/p/pidgin-im-pusher/])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_SRCDIR([impusher.c])

AC_PROG_CC
AC_PROG_LIBTOOL

# pidgin should depend on purple and glib
PKG_CHECK_MODULES(PIDGIN, [pidgin])
AC_SUBST(PIDGIN_CFLAGS)
AC_SUBST(PIDGIN_LIBS)

AC_CHECK_HEADERS([string.h])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
