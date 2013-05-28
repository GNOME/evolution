if OS_WIN32
NO_UNDEFINED_REQUIRED_LIBS = \
	$(GNOME_PLATFORM_LIBS) \
	$(top_builddir)/e-util/libevoluiton-util.la \
	$(top_builddir)/mail/libevolution-mail.la
endif

@EVO_PLUGIN_RULE@

error_DATA = org-gnome-external-editor.error

errordir = $(privdatadir)/errors

plugin_DATA = 						\
	org-gnome-external-editor.eplug

plugin_LTLIBRARIES = liborg-gnome-external-editor.la

liborg_gnome_external_editor_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/composer			\
	$(EVOLUTION_DATA_SERVER_CFLAGS)			\
	$(GNOME_PLATFORM_CFLAGS)			\
	$(GTKHTML_CFLAGS)

liborg_gnome_external_editor_la_SOURCES = 		\
	external-editor.c

liborg_gnome_external_editor_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED)
liborg_gnome_external_editor_la_LIBADD = 			\
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/composer/libevolution-mail-composer.la	\
	$(top_builddir)/shell/libevolution-shell.la		\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la 		\
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la	\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)					\
	$(GTKHTML_LIBS)

EXTRA_DIST = 					\
	org-gnome-external-editor.eplug.xml	\
	org-gnome-external-editor.error.xml

BUILT_SOURCES = org-gnome-external-editor.eplug \
		org-gnome-external-editor.error

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
