# Makefile for libversit.a

# $Id: Makefile.am,v 1.8 2002/03/12 00:41:53 srittau Exp $

VERSIT_SRC =		\
	vcc.y		\
	vcc.h		\
	vobject.c	\
	vobject.h	\
	port.h		\
	vcaltmp.c	\
	vcaltmp.h

noinst_LIBRARIES = libversit.a
noinst_LTLIBRARIES = libversit_lt.la

libversit_a_SOURCES = $(VERSIT_SRC)
libversit_lt_la_SOURCES = $(VERSIT_SRC)

EXTRA_DIST = README.TXT vcaltest.c vctest.c

YFLAGS=-pversit_
