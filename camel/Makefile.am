## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelincludedir = $(includedir)/camel


lib_LTLIBRARIES = libcamel.la

INCLUDES = -I.. -I$(srcdir)/.. -I$(includedir)	\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR)

libcamel_la_SOURCES = 				\
	camel-folder.c	

libcamelinclude_HEADERS =			\
	camel-folder.h	


libcamel_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)


EXTRA_DIST = README
