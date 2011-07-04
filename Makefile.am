## Process this file with automake to produce Makefile.in

sbin_PROGRAMS = nodm

dist_noinst_HEADERS = log.h

nodm_SOURCES = nodm.c
nodm_LDFLAGS = $(PAM_LIBS)
nodm_CFLAGS = -DNODM_SESSION='"$(sbindir)/nodm"'

nodm.8: nodm
	help2man --section=8 --name="X display manager for automatic logins" \
		 --no-info --include=nodm-man-extras ./$< > $@

man_MANS = nodm.8

TESTS = test-xstart
check_PROGRAMS = test-xstart
test_xstart_SOURCES = log.c test-xstart.c

EXTRA_DIST = test_nodm TODO nodm-man-extras autogen.sh

CLEANFILES = nodm.8
