## Process this file with automake to produce Makefile.in

SUBDIRS = providers tests

libcamelincludedir = $(includedir)/camel
providerdir = $(libdir)/evolution/camel-providers/$(VERSION)

lib_LTLIBRARIES = libcamel.la

INCLUDES = -I.. -I$(srcdir)/.. 			 \
	-I$(top_srcdir)/intl			 \
	-I$(top_srcdir)/e-util			 \
	$(GNOME_INCLUDEDIR)			 \
	$(GLIB_CFLAGS)				 \
	$(GNOME_XML_CFLAGS)			 \
	$(UNICODE_CFLAGS)			 \
	$(KRB4_CFLAGS)				 \
	$(NSPR_CFLAGS)				 \
	$(NSS_CFLAGS)				 \
	$(EXTRA_GNOME_CFLAGS)			 \
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\" \
	-DG_LOG_DOMAIN=\"camel\"

libcamel_la_SOURCES = 				\
	broken-date-parser.c			\
	camel-address.c				\
	camel-data-wrapper.c			\
	camel-exception.c			\
	camel-filter-driver.c			\
	camel-filter-search.c			\
	camel-folder-search.c			\
	camel-folder-summary.c			\
	camel-folder-thread.c			\
	camel-folder.c				\
	camel-internet-address.c		\
	camel-lock.c				\
	camel-medium.c				\
	camel-mime-filter-bestenc.c		\
	camel-mime-filter-basic.c		\
	camel-mime-filter-charset.c		\
	camel-mime-filter-crlf.c		\
	camel-mime-filter-from.c		\
	camel-mime-filter-html.c		\
	camel-mime-filter-index.c		\
	camel-mime-filter-linewrap.c		\
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
	camel-operation.c			\
	camel-pgp-context.c			\
	camel-provider.c			\
	camel-remote-store.c			\
	camel-sasl.c				\
	camel-sasl-anonymous.c			\
	camel-sasl-cram-md5.c			\
	camel-sasl-digest-md5.c			\
	camel-sasl-kerberos4.c			\
	camel-sasl-login.c			\
	camel-sasl-plain.c			\
	camel-search-private.c			\
	camel-seekable-stream.c			\
	camel-seekable-substream.c		\
	camel-service.c				\
	camel-session.c				\
	camel-store.c				\
	camel-stream-buffer.c			\
	camel-stream-filter.c			\
	camel-stream-fs.c			\
	camel-stream-mem.c			\
	camel-stream-null.c			\
	camel-stream.c				\
	camel-tcp-stream-raw.c			\
	camel-tcp-stream-ssl.c			\
	camel-tcp-stream-openssl.c		\
	camel-tcp-stream.c			\
	camel-transport.c			\
	camel-uid-cache.c			\
	camel-url.c				\
	camel-vee-folder.c			\
	camel-vee-store.c			\
	camel-vtrash-folder.c			\
	camel-charset-map.c			\
	camel.c					\
	gstring-util.c				\
	hash-table-utils.c			\
	string-utils.c

libcamelinclude_HEADERS =			\
	broken-date-parser.h			\
	camel-address.h				\
	camel-charset-map.h			\
	camel-data-wrapper.h			\
	camel-exception-list.def		\
	camel-exception.h			\
	camel-filter-driver.h			\
	camel-filter-search.h			\
	camel-folder-search.h			\
	camel-folder-summary.h			\
	camel-folder-thread.h			\
	camel-folder.h				\
	camel-internet-address.h		\
	camel-lock.h				\
	camel-medium.h				\
	camel-mime-filter-bestenc.h		\
	camel-mime-filter-basic.h		\
	camel-mime-filter-charset.h		\
	camel-mime-filter-crlf.h		\
	camel-mime-filter-from.h		\
	camel-mime-filter-html.h		\
	camel-mime-filter-index.h		\
	camel-mime-filter-linewrap.h		\
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
	camel-operation.h			\
	camel-pgp-context.h			\
	camel-provider.h			\
	camel-remote-store.h			\
	camel-sasl.h				\
	camel-sasl-anonymous.h			\
	camel-sasl-cram-md5.h			\
	camel-sasl-digest-md5.h			\
	camel-sasl-kerberos4.h			\
	camel-sasl-login.h			\
	camel-sasl-plain.h			\
	camel-seekable-stream.h			\
	camel-seekable-substream.h		\
	camel-service.h				\
	camel-session.h				\
	camel-store.h				\
	camel-stream-buffer.h			\
	camel-stream-filter.h			\
	camel-stream-fs.h			\
	camel-stream-mem.h			\
	camel-stream-null.h			\
	camel-stream.h				\
	camel-tcp-stream-raw.h			\
	camel-tcp-stream-ssl.h			\
	camel-tcp-stream-openssl.h		\
	camel-tcp-stream.h			\
	camel-transport.h			\
	camel-types.h				\
	camel-uid-cache.h			\
	camel-url.h				\
	camel-vee-folder.h			\
	camel-vee-store.h			\
	camel-vtrash-folder.h			\
	camel.h					\
	gstring-util.h				\
	hash-table-utils.h			\
	string-utils.h

libcamel_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)

libcamel_la_LIBADD = $(top_builddir)/e-util/libeutil.la \
	$(UNICODE_LIBS)				\
	$(KRB4_LDFLAGS)				\
	$(NSPR_LDFLAGS)				\
	$(NSS_LDFLAGS)


noinst_HEADERS =				\
	camel-charset-map-private.h		\
	camel-private.h				\
	camel-search-private.h

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
