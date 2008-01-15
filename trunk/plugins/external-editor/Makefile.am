INCLUDES =						\
	-DEVOLUTION_LOCALEDIR=\""$(localedir)"\"	\
	-DEVOLUTION_TOOLSDIR=\""$(privlibexecdir)"\"	\
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/composer			\
	$(SHELL_CFLAGS)					\
	$(EVOLUTION_MAIL_CFLAGS)			\
	$(E_UTIL_CFLAGS)				

@EVO_PLUGIN_RULE@

error_DATA = org-gnome-external-editor.error
errordir = $(privdatadir)/errors

plugin_DATA = 						\
	org-gnome-external-editor.eplug 		\
	org-gnome-external-editor.xml

plugin_LTLIBRARIES = liborg-gnome-external-editor.la

liborg_gnome_external_editor_la_SOURCES = 		\
	external-editor.c

liborg_gnome_external_editor_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = 					\
	org-gnome-external-editor.eplug.xml	\
	org-gnome-external-editor.error.xml	\
	org-gnome-external-editor.xml

BUILT_SOURCES = org-gnome-external-editor.eplug \
		org-gnome-external-editor.error

CLEANFILES = $(BUILT_SOURCES)
