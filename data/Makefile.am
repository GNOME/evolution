desktopdir = $(datadir)/gnome/apps/Applications
desktop_in_files = evolution.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

keysdir = $(datadir)/mime-info
keys_in_files = evolution.keys.in
keys_DATA = $(keys_in_files:.keys.in=.keys) 
@XML_I18N_MERGE_KEYS_RULE@

mimedir = $(datadir)/mime-info
mime_DATA = evolution.mime

EXTRA_DIST = $(desktop_in_files) $(keys_in_files) $(mime_DATA)
