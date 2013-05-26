module_LTLIBRARIES = module-book-config-google.la

module_book_config_google_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-book-config-google\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(GTKHTML_CFLAGS)

module_book_config_google_la_SOURCES = \
	evolution-book-config-google.c

module_book_config_google_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)

module_book_config_google_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
