## Process this file with automake to produce Makefile.in

SUBDIRS = providers

libcamelincludedir = $(includedir)/camel


lib_LTLIBRARIES = libcamel.la

INCLUDES = -I.. -I$(srcdir)/.. -I$(includedir)	\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR)

libcamel_la_SOURCES = 				\
	camel-log.c				\
	camel-data-wrapper.c			\
	camel-folder.c				\
	camel-mime-message.c			\
	camel-mime-part.c			\
	camel-provider.c			\
	camel-service.c				\
	camel-session.c				\
	camel-store.c				\
	gmime-content-field.c			\
	gmime-utils.c				\
	gstring-util.c				\
	url-util.c

libcamelinclude_HEADERS =			\
	camel-log.h				\
	camel-data-wrapper.h			\
	camel-folder.h				\
	camel-mime-message.h			\
	camel-mime-part.h			\
	camel-provider.h			\
	camel-service.h				\
	camel-session.h				\
	camel-store.h				\
	gmime-content-field.h			\
	gmime-utils.h				\
	gstring-util.h				\
	url-util.h


libcamel_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)


EXTRA_DIST = README
