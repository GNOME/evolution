SUBDIRS = icons

desktopdir = $(datadir)/applications
desktop_in_files = evolution.desktop.in evolution-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = evolution-alarm-notify.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

mimedir = $(datadir)/mime-info
dist_mime_DATA = evolution.mime

themedir = $(privdatadir)/theme
dist_theme_DATA =	\
	default.css	\
	tab-bar-background.png \
	tab-switcher.png 	\
	tab-switcher-hover.png	\
	tab-switcher-close.png	\
	tab-switcher-close-hover.png	\
	tab-inactive-hover.png	\
	tab-inactive.png	\
	tab-active.png		\
	tab-inactive-hover-private.png	\
	tab-inactive-private.png	\
	tab-active-private.png	\
	close-on-tab.png	\
	close-on-tab-hover.png	\
	preview-tab-inactive.png	\
	preview-tab-active.png	\
	preview-tab-hover.png	\
	preview-tab-inactive-private.png	\
	preview-tab-active-private.png	\
	preview-tab-hover-private.png	\
	tab-add.png	\
	tab-add-hover.png

if HAVE_KDE_APPLNK

kdedesktopdir = $(KDE_APPLNK_DIR)/Applications
kdedesktop_DATA = $(desktop_in_in_file:.desktop.in.in=.desktop)

endif

dist_noinst_MANS = evolution.1


CLEANFILES =					\
	$(autostart_DATA)			\
	$(desktop_DATA)

EXTRA_DIST =					\
	$(autostart_in_files)			\
	$(desktop_in_files)

-include $(top_srcdir)/git.mk
