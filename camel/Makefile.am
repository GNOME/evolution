## Process this file with automake to produce Makefile.in

SUBDIRS = providers

libcamelincludedir = $(includedir)/camel
providerdir = $(libdir)/evolution/camel-providers/$(VERSION)

lib_LTLIBRARIES = libcamel.la

INCLUDES = -I.. -I$(srcdir)/.. -I$(includedir)	\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR)			\
	$(UNICODE_CFLAGS)			\
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\" \
	-DG_LOG_DOMAIN=\"camel\"

if ENABLE_THREADS

pthread_SRC =	\
	camel-folder-pt-proxy.c		\
	camel-thread-proxy.c

pthread_HDR =	\
	camel-folder-pt-proxy.h			\
	camel-thread-proxy.h	

else

pthread_SRC =
pthread_HDR=

endif


libcamel_la_SOURCES = 				\
	camel.c					\
	camel-data-wrapper.c			\
	camel-exception.c			\
	camel-folder.c				\
	camel-folder-search.c			\
	camel-medium.c				\
	camel-marshal-utils.c			\
	camel-mime-message.c			\
	camel-mime-part.c			\
	camel-mime-part-utils.c			\
	camel-movemail.c			\
	camel-multipart.c			\
	camel-op-queue.c			\
	camel-provider.c			\
	camel-seekable-stream.c			\
	camel-seekable-substream.c		\
	camel-service.c				\
	camel-session.c				\
	camel-store.c				\
	camel-stream.c				\
	camel-stream-buffer.c			\
	camel-stream-fs.c			\
	camel-stream-mem.c			\
	camel-transport.c			\
	camel-url.c				\
	gmime-content-field.c			\
	gstring-util.c				\
	hash-table-utils.c			\
	md5-utils.c				\
	string-utils.c				\
	camel-mime-parser.c			\
	camel-mime-utils.c			\
	camel-mime-filter.c			\
	camel-mime-filter-basic.c		\
	camel-mime-filter-save.c		\
	camel-mime-filter-charset.c		\
	camel-mime-filter-index.c		\
	camel-stream-filter.c			\
	camel-address.c				\
	camel-internet-address.c		\
	$(pthread_SRC)

#	camel-folder-summary.c			\
#	camel-folder-summary.h			\

libcamelinclude_HEADERS =			\
	camel.h					\
	camel-data-wrapper.h			\
	camel-exception.h			\
	camel-folder.h				\
	camel-folder-search.h			\
	camel-marshal-utils.h			\
	camel-medium.h				\
	camel-mime-message.h			\
	camel-mime-part.h			\
	camel-mime-part-utils.h			\
	camel-movemail.h			\
	camel-multipart.h			\
	camel-op-queue.h			\
	camel-provider.h			\
	camel-seekable-stream.h			\
	camel-seekable-substream.h		\
	camel-service.h				\
	camel-session.h				\
	camel-store.h				\
	camel-stream.h				\
	camel-stream-buffer.h			\
	camel-stream-fs.h			\
	camel-stream-mem.h			\
	camel-transport.h			\
	camel-url.h				\
	gmime-content-field.h			\
	gstring-util.h				\
	hash-table-utils.h			\
	md5-utils.h				\
	string-utils.h				\
	camel-exception-list.def		\
	camel-mime-parser.h			\
	camel-mime-utils.h			\
	camel-mime-filter.h			\
	camel-mime-filter-basic.h		\
	camel-mime-filter-save.h		\
	camel-mime-filter-charset.h		\
	camel-mime-filter-index.h		\
	camel-stream-filter.h			\
	camel-address.h				\
	camel-internet-address.h		\
	$(pthread_HDR)

libcamel_extra_sources =			\
	camel-arg-collector.c

libcamel_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)

EXTRA_DIST =					\
	$(libcamel_extra_sources)		\
	README 

noinst_PROGRAMS =				\
	camel-folder-summary

camel_folder_summary_SOURCES = 			\
	camel-folder-summary.c

camel_folder_summary_LDADD = 			\
	../camel/libcamel.la			\
	../e-util/libeutil.la			\
	../libibex/libibex.la			\
	$(GNOME_LIBDIR)				\
	$(GNOMEUI_LIBS)				\
	$(INTLLIBS)				\
	$(PTHREAD_LIB)				\
	$(EXTRA_GNOME_LIBS)

