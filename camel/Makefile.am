## Process this file with automake to produce Makefile.in

SUBDIRS = #providers

libcamelincludedir = $(includedir)/camel


lib_LTLIBRARIES = libcamel.la

INCLUDES = -I.. -I$(srcdir)/.. -I$(includedir)	\
	-I$(top_srcdir)/intl			\
	$(GTK_INCLUDEDIR)


if HAVE_PTHREAD

pthread_SRC =	\
	camel-folder-pt-proxy.c		

pthread_HDR =	\
	camel-folder-pt-proxy.h		

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
	camel-folder.c				\
	camel-folder-summary.c			\
	camel-medium.c				\
	camel-marshal-utils.c			\
	camel-mime-body-part.c			\
	camel-mime-message.c			\
	camel-mime-part.c			\
	camel-mime-part-utils.c			\
	camel-multipart.c			\
	camel-op-queue.c			\
	camel-provider.c			\
	camel-recipient.c			\
	camel-service.c				\
	camel-session.c				\
	camel-store.c				\
	camel-stream.c				\
	camel-stream-buffered-fs.c		\
	camel-stream-fs.c			\
	camel-stream-mem.c			\
	data-wrapper-repository.c		\
	gmime-content-field.c			\
	gmime-utils.c				\
	gstring-util.c				\
	hash-table-utils.c			\
	md5-utils.c				\
	string-utils.c				\
	url-util.c				\
	$(pthread_SRC)

libcamelinclude_HEADERS =			\
	camel.h					\
	camel-log.h				\
	camel-data-wrapper.h			\
	camel-exception.h			\
	camel-simple-data-wrapper.h		\
	camel-folder.h				\
	camel-folder-summary.h			\
	camel-mime-body-part.h			\
	camel-marshal-utils.h			\
	camel-medium.h				\
	camel-mime-message.h			\
	camel-mime-part.h			\
	camel-mime-part-utils.h			\
	camel-multipart.h			\
	camel-op-queue.h			\
	camel-provider.h			\
	camel-recipient.h			\
	camel-service.h				\
	camel-session.h				\
	camel-store.h				\
	camel-stream.h				\
	camel-stream-buffered-fs.h		\
	camel-stream-fs.h			\
	camel-stream-mem.h			\
	data-wrapper-repository.h		\
	gmime-content-field.h			\
	gmime-utils.h				\
	gstring-util.h				\
	hash-table-utils.h			\
	md5-utils.h				\
	string-utils.h				\
	url-util.h				\
	$(pthread_HDR)


libcamel_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)


EXTRA_DIST = README camel-exception-list.def
