## Process this file with automake to produce Makefile.in

SUBDIRS = providers

libcamelincludedir = $(includedir)/camel
providerdir = $(libdir)/evolution/camel-providers/$(VERSION)

lib_LTLIBRARIES = libcamel.la

INCLUDES = -I.. -I$(srcdir)/.. -I$(includedir)	\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR)			\
	-DCAMEL_PROVIDERDIR=\""$(providerdir)"\"


if ENABLE_THREADS

pthread_SRC =	\
	camel-folder-pt-proxy.c		\
	camel-thread-proxy.c

pthread_HDR =	\
	camel-folder-pt-proxy.h			\
	camel-thread-proxy.h	

else

libcamel_pthread_SRC =
libcamel_pthread_HDR=

endif


libcamel_la_SOURCES = 				\
	camel.c					\
	camel-log.c				\
	camel-data-wrapper.c			\
	camel-exception.c			\
	camel-simple-data-wrapper.c		\
	camel-simple-data-wrapper-stream.c	\
	camel-stream-data-wrapper.c		\
	camel-folder.c				\
	camel-folder-summary.c			\
	camel-folder-utils.c			\
	camel-formatter.c			\
	camel-medium.c				\
	camel-marshal-utils.c			\
	camel-mime-body-part.c			\
	camel-mime-message.c			\
	camel-mime-part.c			\
	camel-mime-part-utils.c			\
	camel-movemail.c			\
	camel-multipart.c			\
	camel-op-queue.c			\
	camel-provider.c			\
	camel-recipient.c			\
	camel-seekable-stream.c			\
	camel-seekable-substream.c		\
	camel-service.c				\
	camel-session.c				\
	camel-store.c				\
	camel-stream.c				\
	camel-stream-b64.c			\
	camel-stream-buffer.c			\
	camel-stream-fs.c			\
	camel-stream-mem.c			\
	camel-transport.c			\
	camel-url.c				\
	data-wrapper-repository.c		\
	gmime-base64.c				\
	gmime-content-field.c			\
	gmime-utils.c				\
	gstring-util.c				\
	hash-table-utils.c			\
	md5-utils.c				\
	string-utils.c				\
	$(pthread_SRC)

libcamelinclude_HEADERS =			\
	camel.h					\
	camel-log.h				\
	camel-data-wrapper.h			\
	camel-exception.h			\
	camel-simple-data-wrapper.h		\
	camel-simple-data-wrapper-stream.h	\
	camel-stream-data-wrapper.h		\
	camel-folder.h				\
	camel-folder-summary.h			\
	camel-folder-utils.h			\
	camel-formatter.h			\
	camel-mime-body-part.h			\
	camel-marshal-utils.h			\
	camel-medium.h				\
	camel-mime-message.h			\
	camel-mime-part.h			\
	camel-mime-part-utils.h			\
	camel-movemail.h			\
	camel-multipart.h			\
	camel-op-queue.h			\
	camel-provider.h			\
	camel-recipient.h			\
	camel-seekable-stream.h			\
	camel-seekable-substream.h		\
	camel-service.h				\
	camel-session.h				\
	camel-store.h				\
	camel-stream.h				\
	camel-stream-b64.h			\
	camel-stream-buffer.h			\
	camel-stream-fs.h			\
	camel-stream-mem.h			\
	camel-transport.h			\
	camel-url.h				\
	data-wrapper-repository.h		\
	gmime-base64.h				\
	gmime-content-field.h			\
	gmime-utils.h				\
	gstring-util.h				\
	hash-table-utils.h			\
	md5-utils.h				\
	string-utils.h				\
	camel-exception-list.def		\
	$(pthread_HDR)

libcamel_extra_sources =			\
	camel-arg-collector.c

libcamel_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)

EXTRA_DIST =					\
	$(libcamel_extra_sources)		\
	README 
