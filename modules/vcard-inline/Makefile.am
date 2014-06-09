module_LTLIBRARIES = module-vcard-inline.la

module_vcard_inline_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"		\
	-DG_LOG_DOMAIN=\"evolution-module-vcard-inline\"	\
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)
	$(NULL)

module_vcard_inline_la_SOURCES =				\
	e-mail-formatter-vcard.c				\
	e-mail-formatter-vcard.h				\
	e-mail-parser-vcard.c					\
	e-mail-parser-vcard.h					\
	e-mail-part-vcard.c					\
	e-mail-part-vcard.h					\
	evolution-module-vcard-inline.c

module_vcard_inline_la_LIBADD =						\
	$(top_builddir)/mail/libevolution-mail.la			\
	$(top_builddir)/em-format/libevolution-mail-formatter.la	\
	$(top_builddir)/addressbook/util/libeabutil.la			\
	$(top_builddir)/addressbook/gui/widgets/libeabwidgets.la	\
	$(top_builddir)/addressbook/printing/libecontactprint.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)					\
	$(GNOME_PLATFORM_LIBS)						\
	$(NULL)

module_vcard_inline_la_LDFLAGS =				\
	-avoid-version -module $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

-include $(top_srcdir)/git.mk
