module_LTLIBRARIES = module-book-config-webdav.la

module_book_config_webdav_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-book-config-webdav\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(LIBSOUP_CFLAGS)					\
	$(CODE_COVERAGE_CFLAGS)					\
	$(NULL)

module_book_config_webdav_la_SOURCES = \
	evolution-book-config-webdav.c

module_book_config_webdav_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(LIBSOUP_LIBS)						\
	$(NULL)

module_book_config_webdav_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk
