desktopdir = $(datadir)/gnome/apps/Applications
desktop_in_files = evolution.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

keysdir = $(datadir)/mime-info
keys_in_files = evolution.keys.in
keys_DATA = $(keys_in_files:.keys.in=.keys) 
@INTLTOOL_KEYS_RULE@

mimedir = $(datadir)/mime-info
mime_DATA = evolution.mime

if HAVE_KDE_APPLNK
kdedesktopdir = $(KDE_APPLNK_DIR)/Applications
kdedesktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
endif

man_MANS = evolution.1

EXTRA_DIST = $(desktop_in_files) $(keys_in_files) $(mime_DATA) $(man_MANS)

SUBDIRS = cde_app_root

# CDE Stuff

if HAVE_DTAPPINTEGRATE

# This installs Evolution into the Application Manager on CDE
dtappintegrate:
	$(DTAPPINTEGRATE) -s $(prefix)/share/evolution/cde_app_root

# ...And this uninstalls it
dtappunintegrate:
	$(DTAPPINTEGRATE) -u -s $(prefix)/share/evolution/cde_app_root

endif
