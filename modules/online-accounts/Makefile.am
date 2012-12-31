NULL =

module_LTLIBRARIES = module-online-accounts.la

module_online_accounts_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-online-accounts\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)					\
	$(GOA_CFLAGS)						\
	$(NULL)

module_online_accounts_la_SOURCES = \
	evolution-online-accounts.c				\
	camel-sasl-xoauth.c					\
	camel-sasl-xoauth.h					\
	$(NULL)

module_online_accounts_la_LIBADD = \
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(top_builddir)/libemail-engine/libemail-engine.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)						\
	$(GOA_LIBS)						\
	$(NULL)

module_online_accounts_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
