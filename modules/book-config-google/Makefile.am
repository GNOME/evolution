module_LTLIBRARIES = module-book-config-google.la

module_book_config_google_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-DG_LOG_DOMAIN=\"evolution-book-config-google\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)

module_book_config_google_la_SOURCES = \
	evolution-book-config-google.c

module_book_config_google_la_LIBADD = \
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(top_builddir)/addressbook/printing/libecontactprint.la \
	$(top_builddir)/addressbook/gui/merging/libeabbookmerging.la \
	$(top_builddir)/addressbook/gui/widgets/libeabwidgets.la \
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

module_book_config_google_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
