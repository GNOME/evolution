module_LTLIBRARIES = module-text-highlight.la

module_text_highlight_la_CPPFLAGS =					\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"			\
	-DG_LOG_DOMAIN=\"evolution-module-text-highlight\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)					\
	$(CODE_COVERAGE_CFLAGS)						\
	$(NULL)

module_text_highlight_la_SOURCES =					\
	e-mail-display-popup-text-highlight.c				\
	e-mail-display-popup-text-highlight.h				\
	e-mail-formatter-text-highlight.c				\
	e-mail-formatter-text-highlight.h				\
	e-mail-parser-text-highlight.c					\
	e-mail-parser-text-highlight.h					\
	evolution-module-text-highlight.c				\
	languages.c							\
	languages.h

module_text_highlight_la_LIBADD =					\
	$(top_builddir)/e-util/libevolution-util.la			\
	$(top_builddir)/mail/libevolution-mail.la			\
	$(top_builddir)/em-format/libevolution-mail-formatter.la	\
	$(top_builddir)/shell/libevolution-shell.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(NULL)

module_text_highlight_la_LDFLAGS =					\
	-avoid-version -module $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk
