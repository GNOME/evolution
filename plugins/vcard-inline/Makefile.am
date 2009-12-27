if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(GNOME_PLATFORM_LIBS)
endif

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-vcard-inline.eplug

plugin_LTLIBRARIES = liborg-gnome-vcard-inline.la

liborg_gnome_vcard_inline_la_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/widgets				\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)			\
	$(EVOLUTION_MAIL_CFLAGS)

liborg_gnome_vcard_inline_la_SOURCES = vcard-inline.c

liborg_gnome_vcard_inline_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

liborg_gnome_vcard_inline_la_LIBADD =	\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/addressbook/util/libeabutil.la		\
	$(top_builddir)/addressbook/gui/widgets/libeabwidgets.la		\
	$(top_builddir)/addressbook/gui/merging/libeabbookmerging.la		\
	$(top_builddir)/addressbook/printing/libecontactprint.la		\
	$(EVOLUTION_ADDRESSBOOK_LIBS)				\
	$(EVOLUTION_MAIL_LIBS)					\
	$(GNOME_PLATFORM_LIBS)

EXTRA_DIST = org-gnome-vcard-inline.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
