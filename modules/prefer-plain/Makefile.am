SUBDIRS=plugin

module_LTLIBRARIES = module-prefer-plain.la

module_prefer_plain_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"		\
	-DG_LOG_DOMAIN=\"evolution-module-prefer-plain\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)					\
	$(NULL)

module_prefer_plain_la_SOURCES =				\
	e-mail-parser-prefer-plain.c				\
	e-mail-parser-prefer-plain.h				\
	e-mail-display-popup-prefer-plain.c			\
	e-mail-display-popup-prefer-plain.h			\
	evolution-module-prefer-plain.c

module_prefer_plain_la_LIBADD =						\
	$(top_builddir)/e-util/libevolution-util.la			\
	$(top_builddir)/mail/libevolution-mail.la			\
	$(top_builddir)/em-format/libevolution-mail-formatter.la	\
	$(top_builddir)/shell/libevolution-shell.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(NULL)

module_prefer_plain_la_LDFLAGS =				\
	-avoid-version -module $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk
