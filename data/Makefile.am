SUBDIRS = icons

desktopdir = $(datadir)/applications
desktop_in_files = evolution.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = evolution-alarm-notify.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

gsettings_SCHEMAS =							\
	org.gnome.evolution.gschema.xml					\
	org.gnome.evolution.addressbook.gschema.xml			\
	org.gnome.evolution.calendar.gschema.xml			\
	org.gnome.evolution.importer.gschema.xml			\
	org.gnome.evolution.mail.gschema.xml				\
	org.gnome.evolution.shell.gschema.xml				\
	org.gnome.evolution.bogofilter.gschema.xml			\
	org.gnome.evolution.spamassassin.gschema.xml			\
	org.gnome.evolution.plugin.attachment-reminder.gschema.xml	\
	org.gnome.evolution.plugin.autocontacts.gschema.xml		\
	org.gnome.evolution.plugin.email-custom-header.gschema.xml	\
	org.gnome.evolution.plugin.external-editor.gschema.xml		\
	org.gnome.evolution.plugin.face-picture.gschema.xml		\
	org.gnome.evolution.plugin.itip.gschema.xml			\
	org.gnome.evolution.plugin.mail-notification.gschema.xml	\
	org.gnome.evolution.plugin.prefer-plain.gschema.xml		\
	org.gnome.evolution.plugin.publish-calendar.gschema.xml		\
	org.gnome.evolution.plugin.templates.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = evolution.convert

themedir = $(privdatadir)/theme
dist_theme_DATA =	\
	webview.css	\
	webview-print.css

dist_noinst_MANS = evolution.1


CLEANFILES =					\
	$(autostart_DATA)			\
	$(desktop_DATA)

EXTRA_DIST =					\
	$(autostart_in_files)			\
	$(desktop_in_files)			\
	$(convert_DATA)				\
	$(gsettings_SCHEMAS:.xml=.xml.in)

DISTCLEANFILES =		\
	$(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
