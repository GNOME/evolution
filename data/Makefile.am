keysdir = $(datadir)/mime-info
keys_in_files = evolution.keys.in
keys_DATA = $(keys_in_files:.keys.in=.keys) 

mimedir = $(datadir)/mime-info
mime_DATA = evolution.mime

@XML_I18N_MERGE_KEYS_RULE@

EXTRA_DIST = $(keys_in_files) $(mime_DATA)
