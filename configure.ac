dnl Process this file with autoconf to produce a configure script.
AC_INIT(nodm, 0.3, [enrico@enricozini.org, nomeata@debian.org])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_AUX_DIR([config])
AM_INIT_AUTOMAKE([-Wall std-options foreign])
AM_CONFIG_HEADER(config.h)

dnl AC_GNU_SOURCE
dnl AM_MAINTAINER_MODE

dnl Checks for programs.
AC_PROG_CC
AC_ISC_POSIX

dnl Checks for header files.
AC_HEADER_STDC

dnl Checks for libraries.

AC_CHECK_LIB(pam, pam_start,
	[ PAM_LIBS="-lpam" ],
	AC_MSG_ERROR(libpam is missing)
)

AC_CHECK_LIB(pam_misc, main,
	[ PAM_LIBS="$LIBPAM -lpam_misc" ],
	AC_MSG_ERROR(libpam_misc is missing)
)
AC_SUBST(PAM_LIBS)

AC_CONFIG_FILES([
	Makefile
])
AC_OUTPUT
