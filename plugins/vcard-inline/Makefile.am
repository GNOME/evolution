if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS =				\
	$(top_builddir)/mail/libevolution-mail.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(GNOME_PLATFORM_LIBS)
endif

AM_CPPFLAGS = \
	-I$(top_srcdir)			\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)	\
	$(EVOLUTION_MAIL_CFLAGS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-vcard-inline.eplug
plugin_LTLIBRARIES = liborg-gnome-vcard-inline.la

liborg_gnome_vcard_inline_la_SOURCES = vcard-inline.c
liborg_gnome_vcard_inline_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
liborg_gnome_vcard_inline_la_LIBADD =	\
	$(EVOLUTION_ADDRESSBOOK_LIBS) \
	$(EVOLUTION_MAIL_LIBS)			\
	$(EVOLUTION_ADDRESSBOOK_LIBS) \
	$(NO_UNDEFINED_REQUIRED_LIBS)

EXTRA_DIST = org-gnome-vcard-inline.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
