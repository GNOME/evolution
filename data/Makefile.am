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
