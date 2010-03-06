SUBDIRS = icons

desktopdir = $(datadir)/applications
desktop_in_in_file = evolution.desktop.in.in	\
		     evolution-settings.desktop.in.in
desktop_DATA = $(desktop_in_in_file:.desktop.in.in=.desktop)
%.desktop.in: %.desktop.in.in
	sed -e "s|\@BASE_VERSION\@|$(BASE_VERSION)|" -e "s|\@DATASERVER_EXEC_VERSION\@|$(DATASERVER_EXEC_VERSION)|" $< > $@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_in_files = evolution-alarm-notify.desktop.in.in
autostart_in_files = evolution-alarm-notify.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

keysdir = $(datadir)/mime-info
keys_in_file = evolution.keys.in.in
keys_DATA = $(keys_in_file:.keys.in.in=.keys)
%.keys.in: %.keys.in.in
	sed -e "s|\@BASE_VERSION\@|$(BASE_VERSION)|" $< > $@

@INTLTOOL_KEYS_RULE@

mimedir = $(datadir)/mime-info
dist_mime_DATA = evolution.mime

if HAVE_KDE_APPLNK

kdedesktopdir = $(KDE_APPLNK_DIR)/Applications
kdedesktop_DATA = $(desktop_in_in_file:.desktop.in.in=.desktop)

endif

dist_noinst_MANS = evolution.1


CLEANFILES =					\
	$(autostart_DATA)			\
	$(desktop_DATA)				\
	$(keys_DATA)

EXTRA_DIST =					\
	$(autostart_in_in_files)		\
	$(desktop_in_in_file)			\
	$(keys_in_file)

-include $(top_srcdir)/git.mk
