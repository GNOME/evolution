# Makefile for libversit.a

# $Id: Makefile.am,v 1.11 2002/07/17 21:03:11 peterw Exp $

VERSIT_SRC =		\
	vcc.y		\
	vcc.h		\
	vobject.c	\
	vobject.h	\
	port.h		\
	vcaltmp.c	\
	vcaltmp.h

privlib_LIBRARIES = libversit.a
noinst_LTLIBRARIES = libversit_lt.la

libversit_a_SOURCES = $(VERSIT_SRC)
libversit_lt_la_SOURCES = $(VERSIT_SRC)

EXTRA_DIST = README.TXT vcaltest.c vctest.c

YFLAGS=-pversit_
