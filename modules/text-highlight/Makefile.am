module_LTLIBRARIES = module-text-highlight.la

module_text_highlight_la_CPPFLAGS =					\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/widgets						\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"			\
	-DG_LOG_DOMAIN=\"evolution-module-text-highlight\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)

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
	$(top_builddir)/e-util/libeutil.la				\
	$(top_builddir)/mail/libevolution-mail.la			\
	$(top_builddir)/em-format/libemformat.la			\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)

module_text_highlight_la_LDFLAGS =					\
	-avoid-version -module $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk
