module_LTLIBRARIES = module-book-config-local.la

module_book_config_local_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-DG_LOG_DOMAIN=\"evolution-book-config-local\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)

module_book_config_local_la_SOURCES = \
	evolution-book-config-local.c

module_book_config_local_la_LIBADD = \
	$(top_builddir)/e-util/libeutil.la			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

module_book_config_local_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
