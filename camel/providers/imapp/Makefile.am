## Process this file with automake to produce Makefile.in

camel_provider_LTLIBRARIES = libcamelimapp.la
camel_provider_DATA = libcamelimapp.urls

INCLUDES = -I.. \
	-I$(srcdir)/..				\
	-I$(top_srcdir)/camel			\
	-I$(top_srcdir)/intl			\
	-I$(top_srcdir)/e-util			\
	-I$(top_srcdir)				\
	$(CAMEL_CFLAGS)				\
	$(GNOME_INCLUDEDIR)			\
	$(GTK_INCLUDEDIR)			\
	-DG_LOG_DOMAIN=\"camel-imapp-provider\"

libcamelimapp_la_SOURCES =			\
	camel-imapp-utils.c			\
	camel-imapp-engine.c			\
	camel-imapp-stream.c			\
	camel-imapp-store.c			\
	camel-imapp-folder.c			\
        camel-imapp-provider.c			\
	camel-imapp-store-summary.c		\
	camel-imapp-driver.c			\
	camel-imapp-summary.c

noinst_HEADERS =			\
	camel-imapp-utils.h			\
	camel-imapp-engine.h			\
	camel-imapp-stream.h			\
	camel-imapp-store.h			\
	camel-imapp-folder.h			\
	camel-imapp-store-summary.h		\
	camel-imapp-driver.h			\
	camel-imapp-summary.h

libcamelimapp_la_LDFLAGS = -avoid-version -module

EXTRA_DIST = libcamelimapp.urls
