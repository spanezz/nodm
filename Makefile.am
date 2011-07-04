## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

sbin_PROGRAMS = nodm

dist_noinst_HEADERS = common.h log.h server.h session.h

libsources = common.c log.c server.c session.c

AM_CPPFLAGS = $(X11_CFLAGS)
AM_LDFLAGS = $(PAM_LIBS) $(X11_LIBS)

nodm_SOURCES = $(libsources) nodm.c
nodm_CFLAGS = -DNODM_SESSION='"$(sbindir)/nodm"'

nodm.8: nodm
	help2man --section=8 --name="X display manager for automatic logins" \
		 --no-info --include=nodm-man-extras ./$< > $@

man_MANS = nodm.8

TESTS = test-xstart test-xsession
check_PROGRAMS = test-xstart test-xsession

test_xstart_SOURCES = $(libsources) test-xstart.c

test_xsession_SOURCES = $(libsources) test-xsession.c

EXTRA_DIST = test_nodm TODO nodm-man-extras autogen.sh

CLEANFILES = nodm.8
