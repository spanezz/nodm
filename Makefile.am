## Process this file with automake to produce Makefile.in

NULL =

CFLAGS += -Wall -Werror

ACLOCAL_AMFLAGS = -I m4

sbin_PROGRAMS = nodm

dist_noinst_HEADERS = common.h 		\
                      dm.h		\
                      log.h		\
                      test.h		\
                      vt.h		\
                      xserver.h		\
                      xsession.h	\
                      xsession-child.h	\
                      $(NULL)

libsources = common.c 			\
             log.c			\
             vt.c			\
             xsession-child.c		\
             xserver.c			\
             xsession.c dm.c		\
             $(NULL)

testlibsources = $(libsources)		\
                 test.c			\
                 $(NULL)

AM_CPPFLAGS = $(X11_CFLAGS)
LIBS = $(PAM_LIBS) $(X11_LIBS)

nodm_SOURCES = $(libsources)		\
               nodm.c			\
               $(NULL)

nodm_CFLAGS = -DNODM_SESSION='"$(sbindir)/nodm"'

nodm.8: nodm
	help2man --section=8 --name="X display manager for automatic logins" \
		 --no-info --include=nodm-man-extras ./$< > $@

man_MANS = nodm.8 \
           $(NULL)

TESTS = test-internals test-xstart test-xsession
check_PROGRAMS = test-internals test-xstart test-xsession

test_xstart_SOURCES = $(testlibsources)		\
                      test-xstart.c		\
                      $(NULL)

test_xsession_SOURCES = $(testlibsources) 	\
                        test-xsession.c		\
                        $(NULL)

test_internals_SOURCES = $(testlibsources)	\
                         test-internals.c	\
                         $(NULL)

EXTRA_DIST = test_nodm		\
             nodm-man-extras	\
             autogen.sh		\
             nodm.service.in	\
             $(NULL)

CLEANFILES = $(man_MANS) \
             $(systemdsystemunit_DATA) \
             $(NULL)

DISTCLEANFILES = Makefile.in   \
                 aclocal.m4    \
                 config.h.in   \
                 config/*      \
                 configure     \
                 $(NULL)

systemdsystemunitdir = $(prefix)/lib/systemd/system
systemdsystemunit_DATA = nodm.service

nodm.service: nodm.service.in
	sed \
		-e 's![@]sbindir[@]!${sbindir}!g' \
		< $< > $@
