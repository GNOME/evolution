INCLUDES =						\
	-I$(top_srcdir)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(EVOLUTION_CALENDAR_CFLAGS)			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"

@EVO_PLUGIN_RULE@

plugin_DATA =  org-gnome-evolution-hula-account-setup.eplug
plugin_LTLIBRARIES = liborg-gnome-evolution-hula-account-setup.la 

liborg_gnome_evolution_hula_account_setup_la_SOURCES = \
			camel-hula-listener.c	\
			camel-hula-listener.h	\
			hula-account-setup.c

liborg_gnome_evolution_hula_account_setup_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_evolution_hula_account_setup_la_LIBADD=			\
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/filter/libfilter.la			\
	$(EVOLUTION_CALENDAR_LIBS)				\
	$(EVOLUTION_MAIL_LIBS)

EXTRA_DIST = org-gnome-evolution-hula-account-setup.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)
