## Process this file with automake to produce Makefile.in

SUBDIRS = providers

libcamelincludedir = $(includedir)/camel
providerdir = $(libdir)/evolution/camel-providers/$(VERSION)

lib_LTLIBRARIES = libcamel.la

INCLUDES = -I.. -I$(srcdir)/.. -I$(includedir)	 \
	-I$(top_srcdir)/intl			 \
	$(GLIB_CFLAGS)				 \
	$(UNICODE_CFLAGS)			 \
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\" \
	-DG_LOG_DOMAIN=\"camel\"

libcamel_la_SOURCES = 				\
	broken-date-parser.c			\
	camel-address.c				\
	camel-data-wrapper.c			\
	camel-exception.c			\
	camel-folder-search.c			\
	camel-folder-summary.c			\
	camel-folder.c				\
	camel-internet-address.c		\
	camel-medium.c				\
	camel-mime-filter-basic.c		\
	camel-mime-filter-charset.c		\
	camel-mime-filter-crlf.c		\
	camel-mime-filter-from.c		\
	camel-mime-filter-index.c		\
	camel-mime-filter-save.c		\
	camel-mime-filter.c			\
	camel-mime-message.c			\
	camel-mime-parser.c			\
	camel-mime-part-utils.c			\
	camel-mime-part.c			\
	camel-mime-utils.c			\
	camel-movemail.c			\
	camel-multipart.c			\
	camel-object.c				\
	camel-provider.c			\
	camel-seekable-stream.c			\
	camel-seekable-substream.c		\
	camel-service.c				\
	camel-session.c				\
	camel-store.c				\
	camel-stream-buffer.c			\
	camel-stream-filter.c			\
	camel-stream-fs.c			\
	camel-stream-mem.c			\
	camel-stream.c				\
	camel-transport.c			\
	camel-uid-cache.c			\
	camel-url.c				\
	camel.c					\
	gmime-content-field.c			\
	gstring-util.c				\
	hash-table-utils.c			\
	md5-utils.c				\
	string-utils.c

libcamelinclude_HEADERS =			\
	broken-date-parser.h			\
	camel-address.h				\
	camel-data-wrapper.h			\
	camel-exception-list.def		\
	camel-exception.h			\
	camel-folder-search.h			\
	camel-folder-summary.h			\
	camel-folder.h				\
	camel-internet-address.h		\
	camel-medium.h				\
	camel-mime-filter-basic.h		\
	camel-mime-filter-charset.h		\
	camel-mime-filter-crlf.h		\
	camel-mime-filter-from.h		\
	camel-mime-filter-index.h		\
	camel-mime-filter-save.h		\
	camel-mime-filter.h			\
	camel-mime-message.h			\
	camel-mime-parser.h			\
	camel-mime-part-utils.h			\
	camel-mime-part.h			\
	camel-mime-utils.h			\
	camel-movemail.h			\
	camel-multipart.h			\
	camel-object.h				\
	camel-provider.h			\
	camel-seekable-stream.h			\
	camel-seekable-substream.h		\
	camel-service.h				\
	camel-session.h				\
	camel-store.h				\
	camel-stream-buffer.h			\
	camel-stream-filter.h			\
	camel-stream-fs.h			\
	camel-stream-mem.h			\
	camel-stream.h				\
	camel-transport.h			\
	camel-types.h				\
	camel-uid-cache.h			\
	camel-url.h				\
	camel.h					\
	gmime-content-field.h			\
	gstring-util.h				\
	hash-table-utils.h			\
	md5-utils.h				\
	string-utils.h

libcamel_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)

EXTRA_DIST =					\
	README 

#noinst_PROGRAMS =				\
#	camel-mime-filter-from
#
#camel_mime_filter_from_SOURCES = 		\
#	camel-mime-filter-from.c
#
#camel_mime_filter_from_LDADD = 		\
#	../camel/libcamel.la			\
#	../e-util/libeutil.la			\
#	../libibex/libibex.la			\
#	$(GNOME_LIBDIR)				\
#	$(GNOMEUI_LIBS)				\
#	$(INTLLIBS)				\
#	$(PTHREAD_LIB)				\
#	$(EXTRA_GNOME_LIBS)

