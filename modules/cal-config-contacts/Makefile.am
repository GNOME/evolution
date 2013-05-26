module_LTLIBRARIES = module-cal-config-contacts.la

module_cal_config_contacts_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-cal-config-contacts\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)

module_cal_config_contacts_la_SOURCES = \
	evolution-cal-config-contacts.c				\
	e-contacts-selector.c					\
	e-contacts-selector.h					\
	e-source-contacts.c					\
	e-source-contacts.h

module_cal_config_contacts_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)

module_cal_config_contacts_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
