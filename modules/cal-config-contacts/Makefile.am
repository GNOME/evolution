module_LTLIBRARIES = module-cal-config-contacts.la

module_cal_config_contacts_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-DG_LOG_DOMAIN=\"evolution-cal-config-contacts\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)

module_cal_config_contacts_la_SOURCES = \
	evolution-cal-config-contacts.c				\
	e-contacts-selector.c					\
	e-contacts-selector.h					\
	e-source-contacts.c					\
	e-source-contacts.h

module_cal_config_contacts_la_LIBADD = \
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/addressbook/printing/libecontactprint.la \
	$(top_builddir)/addressbook/gui/merging/libeabbookmerging.la \
	$(top_builddir)/addressbook/gui/widgets/libeabwidgets.la \
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(top_builddir)/calendar/gui/libevolution-calendar.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

module_cal_config_contacts_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
