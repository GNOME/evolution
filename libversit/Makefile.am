# Makefile for libversit.a

# $Id: Makefile.am,v 1.14 2003/10/24 13:05:56 danw Exp $

VERSIT_SRC =		\
	vcc.y		\
	vcc.h		\
	vobject.c	\
	vobject.h	\
	port.h		\
	vcaltmp.c	\
	vcaltmp.h

privlib_LTLIBRARIES = libversit.la

libversit_la_SOURCES = $(VERSIT_SRC)

EXTRA_DIST = README.TXT vcaltest.c vctest.c

AM_YFLAGS=-pversit_
