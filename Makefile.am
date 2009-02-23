## Process this file with automake to produce Makefile.in

sbin_PROGRAMS = nodm

nodm_SOURCES = nodm.c
nodm_LDFLAGS = $(PAM_LIBS)
nodm_CFLAGS = -DNODM_SESSION='"$(sbindir)/nodm"'

EXTRA_DIST = test_nodm TODO
