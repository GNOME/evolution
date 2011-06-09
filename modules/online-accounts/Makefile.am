module_LTLIBRARIES = libevolution-module-online-accounts.la

libevolution_module_online_accounts_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-online-accounts\"		\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)				\
	$(EVOLUTION_CALENDAR_CFLAGS)				\
	$(EVOLUTION_MAIL_CFLAGS)				\
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
	$(GNOME_PLATFORM_LIBS)					\
	$(EVOLUTION_ADDRESSBOOK_LIBS)				\
	$(EVOLUTION_CALENDAR_LIBS)				\
	$(EVOLUTION_MAIL_LIBS)					\
	$(GOA_LIBS)

libevolution_module_online_accounts_la_LDFLAGS =		\
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
