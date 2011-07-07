## Process this file with automake to produce Makefile.in

CFLAGS += -Wall -Werror

ACLOCAL_AMFLAGS = -I m4

sbin_PROGRAMS = nodm

dist_noinst_HEADERS = common.h dm.h log.h test.h vt.h xserver.h xsession.h xsession-child.h

libsources = common.c log.c vt.c xsession-child.c xserver.c xsession.c dm.c
testlibsources = $(libsources) test.c

AM_CPPFLAGS = $(X11_CFLAGS)
AM_LDFLAGS = $(PAM_LIBS) $(X11_LIBS)

nodm_SOURCES = $(libsources) nodm.c
nodm_CFLAGS = -DNODM_SESSION='"$(sbindir)/nodm"'

nodm.8: nodm
	help2man --section=8 --name="X display manager for automatic logins" \
		 --no-info --include=nodm-man-extras ./$< > $@

man_MANS = nodm.8

TESTS = test-internals test-xstart test-xsession
check_PROGRAMS = test-internals test-xstart test-xsession

test_xstart_SOURCES = $(testlibsources) test-xstart.c

test_xsession_SOURCES = $(testlibsources) test-xsession.c

test_internals_SOURCES = $(testlibsources) test-internals.c

EXTRA_DIST = test_nodm nodm-man-extras autogen.sh

CLEANFILES = nodm.8
