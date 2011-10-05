module_LTLIBRARIES = libevolution-module-online-accounts.la

libevolution_module_online_accounts_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-online-accounts\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GOA_CFLAGS)

libevolution_module_online_accounts_la_SOURCES =		\
	evolution-online-accounts.c				\
	e-online-accounts-google.c				\
	e-online-accounts-google.h				\
	camel-sasl-xoauth.c					\
	camel-sasl-xoauth.h

libevolution_module_online_accounts_la_LIBADD =			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/shell/libeshell.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GOA_LIBS)

libevolution_module_online_accounts_la_LDFLAGS =		\
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
