SUBDIRS = local

defaultdir = $(datadir)/evolution/default_user
default_DATA = ldapservers.xml shortcuts.xml

EXTRA_DIST = $(default_DATA)

install-data-hook:
	-mkdir $(defaultdir)/news
