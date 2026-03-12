## Process this file with automake to produce Makefile.in

EXTRA_DIST = CMakeLists.txt

bin_PROGRAMS =		tn5250-headless

LDADD = ../lib5250/lib5250.la

tn5250_headless_SOURCES = 	headlessterm.c tn5250-headless.c

tn5250_headless_CFLAGS = $(AM_CFLAGS)

tn5250_headless_LDFLAGS = -lpthread

pkginclude_HEADERS = 	headlessterm.h

AM_CPPFLAGS = -DSYSCONFDIR=\"$(sysconfdir)\" -I$(top_srcdir)/lib5250
