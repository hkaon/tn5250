## Process this file with automake to produce Makefile.in

EXTRA_DIST = CMakeLists.txt

bin_PROGRAMS =		tn5250

LDADD = ../lib5250/lib5250.la

tn5250_SOURCES = 	cursesterm.c headlessterm.c tn5250.c

tn5250_CFLAGS = $(AM_CFLAGS)

tn5250_LDFLAGS = $(CURSES_LIBS) -lpthread

pkginclude_HEADERS = 	cursesterm.h headlessterm.h

AM_CPPFLAGS = -DSYSCONFDIR=\"$(sysconfdir)\" -I$(top_srcdir)/lib5250
