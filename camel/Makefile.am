## Process this file with automake to produce Makefile.in

SUBDIRS = 

libcamelincludedir = $(includedir)/camel


lib_LTLIBRARIES = libcamel.la

INCLUDES = -I.. -I$(srcdir)/.. -I$(includedir)	\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR)

libcamel_la_SOURCES = 				\
	camel-log.c				\
	camel-folder.c				\
	gstring-util.c

libcamelinclude_HEADERS =			\
	camel-log.h				\
	camel-folder.h				\
	gstring-util.h


libcamel_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)


EXTRA_DIST = README
