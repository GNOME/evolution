## Process this file with automake to produce Makefile.in

libcamelimapincludedir = $(privincludedir)/camel

camel_provider_LTLIBRARIES = libcamelgroupwise.la
camel_provider_DATA = libcamelgroupwise.urls

INCLUDES = \
	-I$(top_srcdir)/camel				\
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir)/servers/groupwise		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/camel/providers/imap		\
	-I$(top_srcdir)/camel/providers/smtp		\
	$(CAMEL_GROUPWISE_CFLAGS)			\
	-DCAMEL_PROVIDERDIR=\"$(camel_providerdir)\"	\
	-DG_LOG_DOMAIN=\"camel-groupwise-provider\"

libcamelgroupwise_la_SOURCES = 			\
	camel-groupwise-provider.c		\
	camel-gw-listener.c 		\
	camel-gw-listener.h		
libcamelgroupwise_la_LIBADD = $(CAMEL_GROUPWISE_LIBS)

libcamelgroupwise_la_LDFLAGS = -avoid-version -module

EXTRA_DIST = libcamelgroupwise.urls

