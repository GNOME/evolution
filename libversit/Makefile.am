# Makefile for libversit.a

# $Id: Makefile.am,v 1.12 2002/07/22 15:35:36 danw Exp $

VERSIT_SRC =		\
	vcc.y		\
	vcc.h		\
	vobject.c	\
	vobject.h	\
	port.h		\
	vcaltmp.c	\
	vcaltmp.h

privlib_LIBRARIES = libversit.a

libversit_a_SOURCES = $(VERSIT_SRC)

EXTRA_DIST = README.TXT vcaltest.c vctest.c

YFLAGS=-pversit_
