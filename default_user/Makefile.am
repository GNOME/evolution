SUBDIRS = local

defaultdir = $(datadir)/evolution/default_user
default_DATA = addressbook-sources.xml searches.xml

EXTRA_DIST = $(default_DATA)

install-data-hook:
if ENABLE_NNTP
	-mkdir $(defaultdir)/news
	-mkdir $(defaultdir)/config
endif
