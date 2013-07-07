if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS = 				\
	$(top_builddir)/e-util/libevolution-util.la	\
	$(top_builddir)/mail/libevolution-mail.la
endif

module_LTLIBRARIES = module-tnef-attachment.la

module_tnef_attachment_la_CPPFLAGS =			\
	$(AM_CPPFLAGS)							\
	-I$(top_srcdir)							\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"			\
	-DG_LOG_DOMAIN=\"evolution-module-tnef-attachment\"		\
	$(EVOLUTION_DATA_SERVER_CFLAGS)					\
	$(GNOME_PLATFORM_CFLAGS)					\
	$(TNEF_CFLAGS)							\
	$(CODE_COVERAGE_CFLAGS)						\
	$(NULL)

module_tnef_attachment_la_SOURCES =			\
	e-mail-parser-tnef-attachment.c					\
	e-mail-parser-tnef-attachment.h					\
	evolution-module-tnef-attachment.c

module_tnef_attachment_la_LIBADD =				\
	$(top_builddir)/e-util/libevolution-util.la			\
	$(top_builddir)/em-format/libevolution-mail-formatter.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	-lytnef								\
	$(NULL)

module_tnef_attachment_la_LDFLAGS =			\
	-avoid-version -module $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk
