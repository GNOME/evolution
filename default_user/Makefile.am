SUBDIRS = local

defaultdir = $(datadir)/evolution/default_user
default_DATA = addressbook-sources.xml searches.xml vfolders.xml

EXTRA_DIST = $(default_DATA)

install-data-hook:
	-mkdir $(defaultdir)/private
if ENABLE_NNTP
	-mkdir $(defaultdir)/news
	-mkdir $(defaultdir)/config
endif
